/*
 * ListenerService.cpp
 *
 *  Created on: 2015年11月13日
 *      Author: zhy
 */

#ifndef LIB_CORE_LISTENERSERVICE_C_
#define LIB_CORE_LISTENERSERVICE_C_

#include "llvm/Support/InstIterator.h"
#include "llvm/IR/Module.h"

#include "ListenerService.h"
#include "PSOListener.h"
#include "SymbolicListener.h"
#include "InputGenListener.h"
#include "Encode.h"
#include <algorithm>

namespace klee {

ListenerService::ListenerService(Executor* executor) {
	runState = 0;
}

void ListenerService::pushListener(BitcodeListener* bitcodeListener) {
	bitcodeListeners.push_back(bitcodeListener);
}

void ListenerService::removeListener(BitcodeListener* bitcodeListener) {
	for (std::vector<BitcodeListener*>::iterator bit = bitcodeListeners.begin(),
			bie = bitcodeListeners.end(); bit != bie; ++bit) {
		if ((*bit) == bitcodeListener) {
			bitcodeListeners.erase(bit);
			break;
		}
	}
}

void ListenerService::popListener() {
	bitcodeListeners.pop_back();
}

RuntimeDataManager* ListenerService::getRuntimeDataManager() {
	return &rdManager;
}

void ListenerService::beforeRunMethodAsMain(ExecutionState &initialState) {
	for (std::vector<BitcodeListener*>::iterator bit = bitcodeListeners.begin(),
			bie = bitcodeListeners.end(); bit != bie; ++bit) {
		(*bit)->beforeRunMethodAsMain(initialState);
	}
}

void ListenerService::executeInstruction(ExecutionState &state,
		KInstruction *ki) {
	for (std::vector<BitcodeListener*>::iterator bit = bitcodeListeners.begin(),
			bie = bitcodeListeners.end(); bit != bie; ++bit) {
		(*bit)->executeInstruction(state, ki);
	}
}

void ListenerService::instructionExecuted(ExecutionState &state,
		KInstruction *ki) {
	for (std::vector<BitcodeListener*>::iterator bit = bitcodeListeners.begin(),
			bie = bitcodeListeners.end(); bit != bie; ++bit) {
		(*bit)->instructionExecuted(state, ki);
	}
}

void ListenerService::afterRunMethodAsMain() {
	for (std::vector<BitcodeListener*>::iterator bit = bitcodeListeners.begin(),
			bie = bitcodeListeners.end(); bit != bie; ++bit) {
		(*bit)->afterRunMethodAsMain();
	}
}

void ListenerService::executionFailed(ExecutionState &state, KInstruction *ki) {
	for (std::vector<BitcodeListener*>::iterator bit = bitcodeListeners.begin(),
			bie = bitcodeListeners.end(); bit != bie; ++bit) {
		(*bit)->executionFailed(state, ki);
	}
}

void ListenerService::createThread(ExecutionState &state, Thread* thread) {
	for (std::vector<BitcodeListener*>::iterator bit = bitcodeListeners.begin(),
			bie = bitcodeListeners.end(); bit != bie; ++bit) {
		(*bit)->createThread(state, thread);
	}
}

void ListenerService::startControl(Executor* executor){
	runState = rdManager.runState;
	switch (runState) {
	case 0: {
		BitcodeListener* listener = new PSOListener(executor, &rdManager);
		pushListener(listener);
		executor->executionNum++;
		gettimeofday(&start, NULL);
		std::cerr << "PSOListener execute.\n";
		break;
	}
	case 1: {
		BitcodeListener* listener = new SymbolicListener(executor, &rdManager);
		pushListener(listener);
		if (executor->prefix) {
			executor->prefix->reuse();
		}
		gettimeofday(&start, NULL);
		std::cerr << "SymbolicListener execute.\n";
		break;
	}
	case 2: {
		BitcodeListener * listener = new InputGenListener(executor, &rdManager);
		pushListener(listener);
		if (executor->prefix) {
			executor->prefix->reuse();
		}
		executor->inputGen = true;
		std::cerr << "InputGenListener execute \n";
		break;
	}
	default: {
		break;
	}
	}
}

void ListenerService::endControl(Executor* executor){
	switch (runState) {
	case 0: {
		popListener();
		gettimeofday(&finish, NULL);
		double cost = (double) (finish.tv_sec * 1000000UL + finish.tv_usec
				- start.tv_sec * 1000000UL - start.tv_usec) / 1000000UL;
		rdManager.runningCost += cost;
		break;
	}
	case 1: {
		// complete Def-Use coverage computing.
		// don't need do anything else.
		//specific input multi-interleaving
		popListener();
		rdManager.runState = 2;
		//comment out to execute the input generate listener.
		markBrOpGloabl(executor);
		Encode encode(&rdManager);
		encode.buildifAndassert();
		if (encode.verify()) {
			encode.check_if();
		}
//		executor->getNewPrefix();
		gettimeofday(&finish, NULL);
		double cost = (double) (finish.tv_sec * 1000000UL + finish.tv_usec
				- start.tv_sec * 1000000UL - start.tv_usec) / 1000000UL;
		rdManager.solvingCost += cost;
		break;
	}
	case 2: {
		popListener();
		//get multi-inputs;
		rdManager.runState = 0;
		//temporally exit exe
//		executor->isFinished = true;
		executor->inputGen = false;
		executor->argvSymbolics.clear();
		break;
	}
	default: {
		break;
	}
	}

}

void ListenerService::changeInputAndPrefix(int argc, char** argv, Executor* executor) {
	if (rdManager.runState == 0 && rdManager.symbolicInputPrefix.size() != 0) {
		std::map<Prefix*, std::vector<std::string> >::iterator it =
				rdManager.symbolicInputPrefix.begin();
		executor->prefix = it->first;
		int i = 1;
		std::vector<std::string>::size_type cnt = it->second.size();
		assert((argc - 1) == cnt && "the number of computed argv is not equal argc");
		unsigned int vecSize = it->second.size();
		for (unsigned j = 0; j < vecSize; j++) {
			unsigned t = 0;
//			std::cerr << "j = " << j << std::endl;
			for (; t < it->second[j].size(); t++)
				argv[i][t] = it->second[j][t];
			argv[i][t] = '\0';
//			std::cerr << "change argv : " << argv[i] << std::endl;
			i++;
		}
		std::map<Prefix*, std::map<std::string, unsigned> >::iterator mit =
				rdManager.intInputPrefix.begin();
		assert(mit->first == it->first && "not the same prefix.\n");
		rdManager.intArgv.clear();
		std::map<std::string, unsigned> tempMap = mit->second;
		rdManager.intArgv.insert(tempMap.begin(), tempMap.end());
		rdManager.intInputPrefix.erase(mit->first);
//		std::cerr << "i = " << i << std::endl;
//		argv[i][0] = '\0';
//		std::cerr << "change input prefix i = " << i << std::endl;
		rdManager.symbolicInputPrefix.erase(it->first);
	}
	rdManager.pArgv = argv;
	rdManager.iArgc = argc;
}

void ListenerService::markBrOpGloabl(Executor *executor) {
	for (std::vector<KFunction*>::iterator it = executor->kmodule->functions.begin(),
			ie = executor->kmodule->functions.end(); it != ie; it++) {
		KInstruction **instructions = (*it)->instructions;
		for (unsigned i = 0; i < (*it)->numInstructions; i++) {
			KInstruction *ki = instructions[i];
			Instruction *inst = ki->inst;

			if (inst->getOpcode() == Instruction::Br) {
				BranchInst *bi = cast<BranchInst>(inst);
				if (bi->isUnconditional())
					continue;
				BranchInst::op_iterator opit = bi->op_begin(), opie = bi->op_end();
				opit++;
				for (; opit != opie; opit++) {
					if (rdManager.bbOpGlobal.find((llvm::BasicBlock*)(opit->get())) !=
							rdManager.bbOpGlobal.end()) {
						ki->operateGlobalVar = true;
						std::cerr << "operate global variables" << std::endl;
						ki->inst->dump();
					}
				}
			}
		}
	}
}


void ListenerService::preparation(Executor *executor) {
	//get global variable name;
	for (Module::const_global_iterator ci = executor->kmodule->module->global_begin(),
			ce = executor->kmodule->module->global_end(); ci != ce; ci++) {
		std::string varName = ci->getName().str();

		if (varName[0] != '.') {
			globalVarNameSet.insert(varName);
			std::cerr << "var name : " << varName << std::endl;
		}
	}

	for (std::vector<KFunction*>::iterator it = executor->kmodule->functions.begin(),
			ie = executor->kmodule->functions.end(); it != ie; it++) {
		KInstruction **instructions = (*it)->instructions;
		for (unsigned i = 0; i < (*it)->numInstructions; i++) {
			KInstruction *ki = instructions[i];
			Instruction *inst = ki->inst;

			if (inst->getOpcode() == Instruction::Br) {
				BranchInst *bi = cast<BranchInst>(inst);
				if (bi->isUnconditional())
					continue;
				BranchInst::op_iterator opit = bi->op_begin(), opie = bi->op_end();
				opit++;
				for (; opit != opie; opit++) {
					BasicBlock *bb = (BasicBlock*)(opit->get());
					if (strncmp((*opit)->getName().str().c_str(), "if.then", 7) == 0) {
						opit->get()->dump();
						std::set<std::string> allGvar;
						ki->trueBT = basicBlockOpGlobal(bb, allGvar);
						if (ki->trueBT == klee::KInstruction::definite) {
							rdManager.bbOpGVarName.insert(make_pair(bb, allGvar));
							rdManager.ifBB.insert(make_pair((*opit)->getName().str(), bb));
						}
					}
					if (strncmp((*opit)->getName().str().c_str(), "if.else", 7) == 0) {
						std::set<std::string> allGvar;
						ki->falseBT = basicBlockOpGlobal(bb, allGvar);
						if (ki->falseBT == klee::KInstruction::definite) {
							rdManager.bbOpGVarName.insert(make_pair(bb, allGvar));
							rdManager.ifBB.insert(make_pair((*opit)->getName().str(), bb));
						}
					}
				}
			}
		}
	}
}

void ListenerService::getMatchingPair(Executor *executor) {
	std::map<std::string, llvm::BasicBlock*>::iterator it =
			rdManager.ifBB.begin(), ie = rdManager.ifBB.end();
	unsigned cnt = 0;


	for (; it != ie; it++, cnt++) {
		std::map<llvm::BasicBlock*, std::set<std::string> >::iterator tempIt =
				rdManager.bbOpGVarName.find(it->second);
		unsigned delta = 0;
		assert(tempIt == rdManager.bbOpGVarName.end());

		std::set<std::string> itBB = tempIt->second;
		std::map<std::string, llvm::BasicBlock*>::iterator innerIt =
				rdManager.ifBB.begin(), innerIe = rdManager.ifBB.end();
		while (delta <= cnt)
			innerIt++;

		for (; innerIt != innerIe; innerIt++) {
			std::map<llvm::BasicBlock*, std::set<std::string> >::iterator
				innerTemp = rdManager.bbOpGVarName.find(innerIt->second);
			assert(innerTemp == rdManager.bbOpGVarName.end());

			std::set<std::string> innerItBB = innerTemp->second;
			unsigned vecSize = itBB.size() > innerItBB.size() ? itBB.size() : innerItBB.size();
			std::set<std::string>::iterator firstIt = itBB.begin(), firstIe = itBB.end();
			std::set<std::string>::iterator secondIt = innerItBB.begin(), secondIe = innerItBB.end();
			std::vector<std::string> intersectRet(vecSize);
			std::vector<std::string>::iterator vecIt;

			vecIt = std::set_intersection(firstIt, firstIe, secondIt, secondIe, intersectRet.begin());

			intersectRet.resize(vecIt - intersectRet.begin());

			if (intersectRet.size() > 0) {
				// a matching pair found. store branches of the two br statement.
				// like pair<if.then, if.then21> pair<if.then, if.else>
				std::string branchName1(it->first);
				std::string branchName2(innerIt->first);
				rdManager.MP.insert(std::make_pair(branchName1, branchName2));
				rdManager.MP.insert(std::make_pair(branchName2, branchName1));
			}
		}
	}
}


KInstruction::BranchType
ListenerService::basicBlockOpGlobal(llvm::BasicBlock *basicBlock, std::set<std::string> &allGvar) {
	KInstruction::BranchType ret = KInstruction::none;

	for (BasicBlock::iterator bit = basicBlock->begin(), bie = basicBlock->end();
			bit != bie; bit++) {
		if (bit->getOpcode() == Instruction::Load) {
			LoadInst *li = dyn_cast<LoadInst>(bit);
			std::string varName = bit->getOperand(0)->getName().str();

			if (li->getType()->getTypeID() == Type::PointerTyID) {
				return klee::KInstruction::possible;
			}

			if (varName != "" && globalVarNameSet.find(varName) != globalVarNameSet.end()) {
				allGvar.insert(varName);
				ret = klee::KInstruction::definite;
			}
		} else if (bit->getOpcode() == Instruction::Store) {
			std::string var1 = bit->getOperand(0)->getName().str();
			std::string var2 = bit->getOperand(1)->getName().str();

			if (var1 != "" && globalVarNameSet.find(var1) != globalVarNameSet.end()) {
				allGvar.insert(var1);
				ret = klee::KInstruction::definite;
			}
			if (var2 != "" && globalVarNameSet.find(var2) != globalVarNameSet.end()) {
				allGvar.insert(var2);
				ret = klee::KInstruction::definite;
			}
		}else if (bit->getOpcode() == Instruction::Br) {
				 BranchInst *bi = cast<BranchInst>(bit);
			if (bi->isUnconditional())
				continue;
			BranchInst::op_iterator opit = bi->op_begin();
			opit++;
			klee::KInstruction::BranchType ret1 =
					basicBlockOpGlobal((BasicBlock*)(opit->get()), allGvar);
			opit++;
			klee::KInstruction::BranchType ret2 =
					basicBlockOpGlobal((BasicBlock*)(opit->get()), allGvar);
			if (ret1 == klee::KInstruction::possible ||
					ret2 == klee::KInstruction::possible)
				return klee::KInstruction::possible;
			if (ret1 == klee::KInstruction::definite ||
					ret2 == klee::KInstruction::definite)
				ret = klee::KInstruction::definite;
		} else if (bit->getOpcode() == Instruction::Call) {
			CallInst* ci = cast<CallInst>(bit);
			unsigned argvs = ci->getNumArgOperands();

			for (unsigned i = 0; i < argvs; i++) {
				if (ci->getOperand(i)->getType()->getTypeID() == Type::PointerTyID) {
					return klee::KInstruction::possible;
				}
			}

			Function* func = ci->getCalledFunction();
			std::set<std::string> funcNameSet;

			if (!func->isDeclaration()) {
				std::set<std::string> opGVar;
				klee::KInstruction::BranchType temp = funcOpGlobal(funcNameSet, func, opGVar);
				if (temp == klee::KInstruction::possible)
					return klee::KInstruction::possible;
				if (temp == klee::KInstruction::definite) {
					ret = temp;
					allGvar.insert(opGVar.begin(), opGVar.end());
				}
			}
		}
	}

	return ret;
}

klee::KInstruction::BranchType
ListenerService::funcOpGlobal(std::set<std::string> &funcNameSet,
		llvm::Function *func, std::set<std::string> &opGVar) {
	std::string funcName = func->getName().str();
	if (funcNameSet.size() > 0 && funcNameSet.find(funcName) != funcNameSet.end())
		return klee::KInstruction::none;

	klee::KInstruction::BranchType ret = klee::KInstruction::none;

	funcNameSet.insert(funcName);
	for (inst_iterator it = inst_begin(func), ie = inst_end(func); it != ie; it++) {
		if (ret == klee::KInstruction::possible)
			return klee::KInstruction::possible;

		Instruction *inst = &*it;
		if (inst->getOpcode() == Instruction::Load) {
			// Load ** or more than return possible.
			LoadInst *li = dyn_cast<LoadInst>(inst);

			if (li->getType()->getTypeID() == Type::PointerTyID) {
				return klee::KInstruction::possible;
			}
			std::string varName = inst->getOperand(0)->getName().str();
			if (varName != "" && globalVarNameSet.find(varName) != globalVarNameSet.end()) {
				opGVar.insert(varName);
				ret = klee::KInstruction::definite;
			}
		} else if (inst->getOpcode() == Instruction::Store) {
			std::string var1 = inst->getOperand(0)->getName().str();
			std::string var2 = inst->getOperand(1)->getName().str();

			if (var1 != "" && globalVarNameSet.find(var1) != globalVarNameSet.end()) {
				opGVar.insert(var1);
				ret = klee::KInstruction::definite;
			}
			if (var2 != "" && globalVarNameSet.find(var2) != globalVarNameSet.end()) {
				opGVar.insert(var2);
				ret = klee::KInstruction::definite;
			}
		} else if (inst->getOpcode() == Instruction::Call) {
			CallInst* ci = cast<CallInst>(inst);
			Function* func = ci->getCalledFunction();

			if (!func->isDeclaration()) {
				klee::KInstruction::BranchType temp = funcOpGlobal(funcNameSet, func, opGVar);
				if (temp == klee::KInstruction::possible)
						return klee::KInstruction::possible;
				if (temp == klee::KInstruction::definite)
					ret = klee::KInstruction::definite;
			}
		}
	}
	return ret;
}

}

#endif
