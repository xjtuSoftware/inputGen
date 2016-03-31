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
//		markBrOpGloabl(executor);
		Encode encode(&rdManager);
		encode.buildifAndassert();
//		encode.getPrefixForDefUse();
//		/***
		if (encode.verify()) {
			encode.check_if();
		}
//		*****/
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
	std::cerr << "prefix set size = " << rdManager.getPrefixSetSize() << endl;
	if (rdManager.runState == 0 && rdManager.symbolicInputPrefix.size() != 0) {
		std::map<Prefix*, std::vector<std::string> >::iterator it =
				rdManager.symbolicInputPrefix.begin();
		std::cerr << "size of prefix and input : " << rdManager.symbolicInputPrefix.size() << endl;
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
	std::cerr << "argc = " << argc << endl;
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

std::string ListenerService::getBlockFullName(llvm::BranchInst *bi, bool brCond) {
	std::string ret = "";

	std::string blockName = "";
	if (brCond)
		blockName = bi->getOperand(2)->getName().str();
	else
		blockName = bi->getOperand(1)->getName().str();

	std::string funcName = bi->getParent()->getParent()->getName().str();

	ret = funcName + "." + blockName;

	return ret;
}

KInstruction::BranchType ListenerService::instOpGlobal(
		Instruction *inst, std::set<std::string> &gVarNames) {
	KInstruction::BranchType ret = KInstruction::none;
	if (inst->getOpcode() == Instruction::Load) {
		LoadInst *li = dyn_cast<LoadInst>(inst);
		if (li->getType()->getTypeID() == Type::PointerTyID)
			return KInstruction::possible;
		std::string varName = inst->getOperand(0)->getName().str();

		if (varName != "" && globalVarNameSet.find(varName) != globalVarNameSet.end()) {
			gVarNames.insert(varName);
			ret = KInstruction::definite;
		}
	} else if (inst->getOpcode() == Instruction::Store) {
		StoreInst *si = dyn_cast<StoreInst>(inst);
		std::string var1 = inst->getOperand(0)->getName().str();
		std::string var2 = inst->getOperand(1)->getName().str();

		if (var1 != "" && globalVarNameSet.find(var1) != globalVarNameSet.end()) {
			gVarNames.insert(var1);
			ret = KInstruction::definite;
		}
		if (var2 != "" && globalVarNameSet.find(var2) != globalVarNameSet.end()) {
			gVarNames.insert(var2);
			ret = KInstruction::definite;
		}
	}

	return ret;
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
				if (strncmp(bi->getOperand(1)->getName().str().c_str(), "if.else", 7) == 0 &&
						strncmp(bi->getOperand(2)->getName().str().c_str(), "if.then", 7) == 0 &&
						strncmp(bi->getParent()->getParent()->getName().str().c_str(), "klee", 4) != 0) {
				bool isPossible = false;
				std::set<std::string> gVarNames;
				llvm::Instruction *optiInst = (llvm::Instruction *)bi->getOperand(0);
				optiInst = optiInst->getNextNode();
				while (optiInst->getOpcode() != Instruction::Br) {
					KInstruction::BranchType temp = instOpGlobal(optiInst, gVarNames);
					if (temp == KInstruction::possible) {
						isPossible = true;
						break;
					}
					optiInst = optiInst->getNextNode();
				}
				if (isPossible) {
					std::cerr << "that's possible branch." << endl;
					ki->trueBT = KInstruction::possible;
					ki->falseBT = KInstruction::possible;
				} else {
					// handle true block.
					bi->dump();
					std::set<std::string> thenNames;
					thenNames.insert(gVarNames.begin(), gVarNames.end());
					std::string thenName = bi->getOperand(2)->getName().str();
					std::cerr << "^^^^^^^^^^^^^^^^^^^^^^^^^^" << endl;
					std::cerr << "then block name : " << thenName << endl;
					BasicBlock *bbThen = bi->getSuccessor(0);
					ki->trueBT = basicBlockOpGlobal(bbThen, thenNames);
					std::set<std::string>::iterator thenIt = thenNames.begin(),
							thenIe = thenNames.end();
					for (; thenIt != thenIe; thenIt++) {
						std::cerr << "then global var: " << *thenIt << endl;
					}
					std::string fullThen = getBlockFullName(bi, true);
					std::cerr << "true full name : " << fullThen << endl;
					std::cerr << "ki->trueBT : " << ki->trueBT << endl;
					std::cerr << "=========================" << endl;
					if (ki->trueBT == KInstruction::definite) {
						rdManager.bbOpGVarName.insert(make_pair(bbThen, thenNames));
						rdManager.ifBB.insert(make_pair(fullThen, bbThen));
					}

					// handle else block
					std::set<std::string> elseNames;
					elseNames.insert(gVarNames.begin(), gVarNames.end());
					std::string elseName = bi->getOperand(1)->getName().str();
					std::cerr << "^^^^^^^^^^^^^^^^^^^^^^^^^^" << endl;
					std::cerr << "else block name : " << elseName << endl;
					BasicBlock *bbElse = bi->getSuccessor(1);
					ki->falseBT = basicBlockOpGlobal(bbElse, elseNames);
					std::string fullElse = getBlockFullName(bi, false);
					std::set<std::string>::iterator elseIt = elseNames.begin(),
							elseIe = elseNames.end();
					for (; elseIt != elseIe; elseIt++) {
						std::cerr << "else global var: " << *elseIt << endl;
					}
					std::cerr << "false full name : " << fullElse << endl;
					std::cerr << "ki->falseBT : " << ki->falseBT << endl;
					std::cerr << "=========================" << endl;
					if (ki->falseBT == KInstruction::definite) {
						rdManager.bbOpGVarName.insert(make_pair(bbElse, elseNames));
						rdManager.ifBB.insert(make_pair(fullElse, bbElse));
					}
				}
			}
		}
		}
	}
	getMatchingPair(executor);
}

void ListenerService::getMatchingPair(Executor *executor) {
	std::map<std::string, llvm::BasicBlock*>::iterator it =
			rdManager.ifBB.begin(), ie = rdManager.ifBB.end();
	std::cerr << "ifBB size = " << rdManager.ifBB.size() << ", " <<
			", bbOpGVarName : " << rdManager.bbOpGVarName.size() << endl;
	unsigned cnt = 0;


	for (; it != ie; it++, cnt++) {
		std::map<llvm::BasicBlock*, std::set<std::string> >::iterator tempIt =
				rdManager.bbOpGVarName.find(it->second);
		unsigned delta = 0;
		unsigned pos = 0;
		if (tempIt == rdManager.bbOpGVarName.end()) {
			std::cerr << "do not have corresponding string set." << endl;
			assert(0 && "do not have corresponding string set.\n");
		}

		std::set<std::string> itBB = tempIt->second;
		std::map<std::string, llvm::BasicBlock*>::iterator innerIt =
				rdManager.ifBB.begin(), innerIe = rdManager.ifBB.end();
		while (delta <= cnt) {
			innerIt++;
			delta++;
		}
		while (it->first[pos] != '.') {
			pos++;
		}
		for (; innerIt != innerIe; innerIt++) {
			std::map<llvm::BasicBlock*, std::set<std::string> >::iterator
				innerTemp = rdManager.bbOpGVarName.find(innerIt->second);
			if (innerTemp == rdManager.bbOpGVarName.end()) {
				std::cerr << "do not have corresponding string set innner." << endl;
				assert(0 && "do not have corresponding string set innner.\n");
			}
			if (strncmp(it->first.c_str(), innerIt->first.c_str(), pos) == 0)
				continue;

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

				std::cerr << "MP first : " << branchName1 <<
						", second : " << branchName2 << endl;
				std::cerr << "MP size = " << rdManager.MP.size() << endl;
			}
		}
	}
}


KInstruction::BranchType
ListenerService::basicBlockOpGlobal(llvm::BasicBlock *basicBlock, std::set<std::string> &allGvar) {
	KInstruction::BranchType ret = KInstruction::none;

	for (BasicBlock::iterator bit = basicBlock->begin(), bie = basicBlock->end();
			bit != bie; bit++) {
//		bit->dump();
		if (bit->getOpcode() == Instruction::Load) {
			LoadInst *li = dyn_cast<LoadInst>(bit);
			std::string varName = bit->getOperand(0)->getName().str();
			std::cerr << "Load name : " << varName << endl;

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
			klee::KInstruction::BranchType ret1 =
					basicBlockOpGlobal(bi->getSuccessor(0), allGvar);
			klee::KInstruction::BranchType ret2 =
					basicBlockOpGlobal(bi->getSuccessor(1), allGvar);

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
