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
		int vecSize = it->second.size();
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
					if (strncmp((*opit)->getName().str().c_str(), "if.then", 7) == 0) {
						ki->trueBlockHasGlobal = basicBlockOpGlobal((BasicBlock*)(opit->get()));
						if (ki->trueBlockHasGlobal) {
							inst->dump();
							std::cerr << " then block has global vars." << ki->info->line << std::endl;
						}
					}
					if (strncmp((*opit)->getName().str().c_str(), "if.else", 7) == 0) {
						ki->falseBlockHasGlobal = basicBlockOpGlobal((BasicBlock*)(opit->get()));
						if (ki->falseBlockHasGlobal) {
							inst->dump();
							std::cerr << " else block has global vars." << ki->info->line << std::endl;
						}
					}
				}
			}
		}
	}
}


bool ListenerService::basicBlockOpGlobal(llvm::BasicBlock *basicBlock) {
	bool ret = false;
	for (BasicBlock::iterator bit = basicBlock->begin(), bie = basicBlock->end();
			bit != bie; bit++) {
		if (bit->getOpcode() == Instruction::Load) {
			std::string varName = bit->getOperand(0)->getName().str();
//			bit->dump();
//			std::cerr << "Load operand 0 value id = " << bit->getOperand(0)->getValueID() << std::endl;
			if (varName != "" && globalVarNameSet.find(varName) != globalVarNameSet.end()) {
				ret = true;
				break;
			}
		} else if (bit->getOpcode() == Instruction::Store) {
			std::string var1 = bit->getOperand(0)->getName().str();
			std::string var2 = bit->getOperand(1)->getName().str();
			std::cerr << "var1 = " << var1 << ", var2 = " << var2 << std::endl;
			if (var1 != "" && globalVarNameSet.find(var1) != globalVarNameSet.end()) {
				ret = true;
				break;
			}
			if (var2 != "" && globalVarNameSet.find(var2) != globalVarNameSet.end()) {
				ret = true;
				break;
			}
		}else if (bit->getOpcode() == Instruction::Br) {
				 BranchInst *bi = cast<BranchInst>(bit);
			if (bi->isUnconditional())
				continue;
			BranchInst::op_iterator opit = bi->op_begin(), opie = bi->op_end();
			opit++;
			for (; opit != opie; opit++) {
				 if (strncmp((*opit)->getName().str().c_str(), "if.then", 7) == 0 ||
						 strncmp((*opit)->getName().str().c_str(), "if.else", 7) == 0) {
					 ret = basicBlockOpGlobal((BasicBlock*)(opit->get()));
				}
			}
		} else if (bit->getOpcode() == Instruction::Call) {
			CallInst* ci = cast<CallInst>(bit);
			Function* func = ci->getCalledFunction();
			std::set<std::string> funcNameSet;
			if (!func->isDeclaration())
				ret = funcOpGlobal(funcNameSet, func);
		}
	}
	return ret;
}

bool ListenerService::funcOpGlobal(std::set<std::string> &funcNameSet, llvm::Function *func) {
	std::string funcName = func->getName().str();
	if (funcNameSet.size() > 0 && funcNameSet.find(funcName) == funcNameSet.end())
		return false;
	bool ret = false;
	funcNameSet.insert(funcName);
	for (inst_iterator it = inst_begin(func), ie = inst_end(func); it != ie; it++) {
		Instruction *inst = &*it;
		if (inst->getOpcode() == Instruction::Load) {
			std::string varName = inst->getOperand(0)->getName().str();
			if (varName != "" && globalVarNameSet.find(varName) != globalVarNameSet.end()) {
				ret = true;
				break;
			}
		} else if (inst->getOpcode() == Instruction::Store) {
			std::string var1 = inst->getOperand(0)->getName().str();
			std::string var2 = inst->getOperand(1)->getName().str();
			if (var1 != "" && globalVarNameSet.find(var1) != globalVarNameSet.end()) {
				ret = true;
				break;
			}
			if (var2 != "" && globalVarNameSet.find(var2) != globalVarNameSet.end()) {
				ret = true;
				break;
			}
		} else if (inst->getOpcode() == Instruction::Call) {
			CallInst* ci = cast<CallInst>(inst);
			Function* func = ci->getCalledFunction();
			if (!func->isDeclaration()) {
				ret = funcOpGlobal(funcNameSet, func);
			}
		}
	}
	return ret;
}

}

#endif
