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
		gettimeofday(&start, NULL);
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
//		std::cerr << "listener read set size:" << rdManager.getCurrentTrace()->readSet.size() << std::endl;
//		std::map<std::string, std::vector<Event *> >::iterator itr = rdManager.getCurrentTrace()->readSet.begin(),
//				ier = rdManager.getCurrentTrace()->readSet.end();
//		unsigned cntR = 0;
//		for (; itr != ier; itr++) {
//			std::cerr << "var Name : " << itr->first << std::endl;
//			cntR += itr->second.size();
//		}
//		std::cerr << "size = " << cntR << std::endl;
//		std::cout << "listener write set size:" << rdManager.getCurrentTrace()->writeSet.size() << std::endl;
//		std::map<std::string, std::vector<Event *> >::iterator itw = rdManager.getCurrentTrace()->writeSet.begin(),
//					iew = rdManager.getCurrentTrace()->writeSet.end();
//			unsigned cntW = 0;
//			for (; itw != iew; itw++) {
//				std::cerr << "var Name : " << itw->first << std::endl;
//				cntW += itw->second.size();
//			}
//		std::cerr << "size = " << cntW << std::endl;
		encode.buildifAndassert();

		DefUseBuilder duBuilder(rdManager, encode);
		duBuilder.buildAllDefUse();
//		string ErrorInfo;
//		raw_fd_ostream out_to_file("./output_info/first.txt", ErrorInfo, 0x0202);
//		stringstream duss;
//		for (unsigned i = 0; i < rdManager.explicitDefUse_pre.size(); i++) {
//			duss << " i = " << i << "\n";
//			if (rdManager.explicitDefUse_pre[i]->pre != NULL) {
//				duss << rdManager.explicitDefUse_pre[i]->pre->toString() << " ";
//			}
//				duss << rdManager.explicitDefUse_pre[i]->post->toString() << "\n";
//
//
//			if (rdManager.explicitDefUse_pre[i]->pre == NULL) {
//				duss << "NULL " << " ";
//			} else {
//				duss << rdManager.explicitDefUse_pre[i]->pre->inst->info->assemblyLine << " ";
//			}
//				duss << rdManager.explicitDefUse_pre[i]->post->inst->info->assemblyLine << "\n";
//		}
//		out_to_file << duss.str();
//		out_to_file.close();
		std::cerr << "covered du : " << rdManager.explicitDefUse_pre.size() << endl;
		std::cerr << "compute du : " << rdManager.coveredDefUse_pre.size() << endl;
//		encode.getPrefixForDefUse();
		encode.getPrefixFromMP();
		/***
		if (encode.verify()) {
			encode.check_if();
		}
		*****/
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

		gettimeofday(&finish, NULL);
		double cost = (double) (finish.tv_sec * 1000000UL + finish.tv_usec
				- start.tv_sec * 1000000UL - start.tv_usec) / 1000000UL;
		rdManager.inputCost += cost;
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
//		assert((unsigned long)(argc - 1) == cnt && "the number of computed argv is not equal argc");
		unsigned int vecSize = it->second.size();
		for (unsigned j = 0; j < vecSize; j++) {
			unsigned t = 0;
			for (; t < it->second[j].size(); t++)
				argv[i][t] = it->second[j][t];
			argv[i][t] = '\0';
			i++;
		}
		std::map<Prefix*, std::map<std::string, unsigned> >::iterator mit =
				rdManager.intInputPrefix.begin();
		assert(mit->first == it->first && "not the same prefix.\n");
		rdManager.intArgv.clear();
		std::map<std::string, unsigned> tempMap = mit->second;
		rdManager.intArgv.insert(tempMap.begin(), tempMap.end());
		rdManager.intInputPrefix.erase(mit->first);
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
//						std::cerr << "operate global variables" << std::endl;
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

KInstruction::BranchType
ListenerService::processEntryBlock(llvm::BasicBlock *bb) {
	// handle entry BasicBlock.
//	std::cerr << "entry name : " << bb->getName().str() << endl;
	KInstruction::BranchType ret = KInstruction::none;
	BasicBlock::iterator bit = bb->begin(), bie = bb->end(), bbIt = bb->end();
	std::set<std::string> gVarNames;
	llvm::Instruction *optiInst = NULL;

	bbIt--;


	llvm::Instruction *lastInst = (llvm::Instruction *)(bbIt);
	if (lastInst->getOpcode() == Instruction::Br) {
		llvm::BranchInst *brInst = dyn_cast<BranchInst>(lastInst);
		if (brInst->isUnconditional()) {
			optiInst = lastInst;
		} else {
			optiInst = (llvm::Instruction *)bbIt->getOperand(0);
		}
	} else {
		optiInst = dyn_cast<Instruction>(bbIt);
	}

	llvm::Instruction *tempInst = dyn_cast<Instruction>(bit);

	while (tempInst != optiInst) {
		if (bit->getOpcode() == Instruction::Load) {
			LoadInst *li = dyn_cast<LoadInst>(bit);
			std::string varName = bit->getOperand(0)->getName().str();
//			std::cerr << "Load name : " << varName << endl;

			if (li->getType()->getTypeID() == Type::PointerTyID) {
				ret =  klee::KInstruction::possible;
				break;
			}

			if (varName != "" && globalVarNameSet.find(varName) != globalVarNameSet.end()) {
				gVarNames.insert(varName);
				ret = klee::KInstruction::definite;
			}
		} else if (bit->getOpcode() == Instruction::Store) {
			std::string var1 = bit->getOperand(0)->getName().str();
			std::string var2 = bit->getOperand(1)->getName().str();

			if (bit->getOperand(0)->getType()->getTypeID() == Type::PointerTyID) {
				ret =  klee::KInstruction::possible;
				break;
			}
			if (var1 != "" && globalVarNameSet.find(var1) != globalVarNameSet.end()) {
				gVarNames.insert(var1);
				ret = klee::KInstruction::definite;
			}
			if (var2 != "" && globalVarNameSet.find(var2) != globalVarNameSet.end()) {
				gVarNames.insert(var2);
				ret = klee::KInstruction::definite;
			}
		} else if (bit->getOpcode() == Instruction::Call) {
			CallInst* ci = cast<CallInst>(bit);
			unsigned argvs = ci->getNumArgOperands();
			Function* func = ci->getCalledFunction();
//			std::cerr << "name : " << func->getName().str() << endl;
			if (!(func == NULL)) {
				if (func->getName().str() == "printf" ||
						func->getName().str() == "sprintf" ||
						func->getName().str() == "fprintf") {
					;
				} else {
					for (unsigned i = 0; i < argvs; i++) {
						if (ci->getOperand(i)->getType()->getTypeID() == Type::PointerTyID) {
							return klee::KInstruction::possible;
						}
					}

					std::set<std::string> funcNameSet;

					if (!func->isDeclaration()) {
						std::set<std::string> opGVar;
						klee::KInstruction::BranchType temp = funcOpGlobal(funcNameSet, func, opGVar);
						if (temp == klee::KInstruction::possible) {
							ret = klee::KInstruction::possible;
							break;
						}
						if (temp == klee::KInstruction::definite) {
							ret = temp;
							gVarNames.insert(opGVar.begin(), opGVar.end());
						}
					}
				}
			}
		}
		bit++;
		tempInst = dyn_cast<Instruction>(bit);
	}
	if (ret == KInstruction::definite) {
		std::string mp1Name = bb->getParent()->getName().str() + "." +
				bb->getName().str();
		rdManager.bbOpGVarName.insert(make_pair(bb, gVarNames));
		rdManager.ifBB.insert(make_pair(mp1Name, bb));
	}
//	std::cerr << "entry block:" << endl;
//	for (std::set<std::string>::iterator it = gVarNames.begin(),
//			ie = gVarNames.end(); it != ie; it++) {
//		std::cerr << "var : " << *it << endl;
//	}

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
//		std::cerr << "entry block name : " << (*it)->function->getName().str() << endl;
//		(*it)->function->begin()->dump();
		processEntryBlock((BasicBlock *)(*it)->function->begin());
		KInstruction **instructions = (*it)->instructions;
		for (unsigned i = 0; i < (*it)->numInstructions; i++) {
			KInstruction *ki = instructions[i];
			Instruction *inst = ki->inst;
			std::set<std::string> bbNames;
			if (inst->getOpcode() == Instruction::Br &&
					!belongToRuntimeDir(ki->info->file)) {
				BranchInst *bi = cast<BranchInst>(inst);
				if (bi->isUnconditional() &&
						strncmp(bi->getParent()->getParent()->getName().str().c_str(), "klee", 4) != 0) {
//					std::cerr << "unconditional jump" << endl;
//					bi->getSuccessor(0)->dump();
					std::set<string> nameSet;
					BasicBlock *uncondBB = bi->getSuccessor(0);
//					std::cerr << "uncond bb name : " << uncondBB->getName().str() << endl;
					if (strncmp(uncondBB->getParent()->getName().str().c_str(), "klee", 4) != 0) {
						bbNames.clear();
						KInstruction::BranchType temp  = basicBlockOpGlobal(uncondBB, nameSet, bbNames);
						if (temp == KInstruction::definite) {
							std::string mp1Name = uncondBB->getParent()->getName().str() +
									"." + uncondBB->getName().str();
							rdManager.bbOpGVarName.insert(make_pair(uncondBB, nameSet));
							rdManager.ifBB.insert(make_pair(mp1Name, uncondBB));

						}
					}
					continue;
				}
				if (/*strncmp(bi->getOperand(1)->getName().str().c_str(), "if.else", 7) == 0 &&
						strncmp(bi->getOperand(2)->getName().str().c_str(), "if.then", 7) == 0 && */
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
//					std::cerr << "that's possible branch." << endl;
					ki->trueBT = KInstruction::possible;
					ki->falseBT = KInstruction::possible;
				} else {
					// handle true block.
//					bi->dump();
					std::set<std::string> thenNames;
					thenNames.insert(gVarNames.begin(), gVarNames.end());
					std::string thenName = bi->getOperand(2)->getName().str();
//					std::cerr << "^^^^^^^^^^^^^^^^^^^^^^^^^^" << endl;
//					std::cerr << "then block name : " << thenName << endl;
					BasicBlock *bbThen = bi->getSuccessor(0);
					bbNames.clear();
					ki->trueBT = basicBlockOpGlobal(bbThen, thenNames, bbNames);
//					std::set<std::string>::iterator thenIt = thenNames.begin(),
//							thenIe = thenNames.end();
////					for (; thenIt != thenIe; thenIt++) {
////						std::cerr << "then global var: " << *thenIt << endl;
////					}
					std::string fullThen = getBlockFullName(bi, true);
//					std::cerr << "true full name : " << fullThen << endl;
//					bi->dump();
//					std::cerr << "file name bool value : " << belongToRuntimeDir(ki->info->file) << endl;
//					std::cerr << "file = " << ki->info->file << ", line = " <<
//							ki->info->line << ", id = " << ki->info->id << endl;
//					std::cerr << "ki->trueBT : " << ki->trueBT << endl;
//					std::cerr << "=========================" << endl;
					if (ki->trueBT == KInstruction::definite) {
						rdManager.bbOpGVarName.insert(make_pair(bbThen, thenNames));
						rdManager.ifBB.insert(make_pair(fullThen, bbThen));
					}

					// handle else block
					std::set<std::string> elseNames;
					elseNames.insert(gVarNames.begin(), gVarNames.end());
					std::string elseName = bi->getOperand(1)->getName().str();
//					std::cerr << "^^^^^^^^^^^^^^^^^^^^^^^^^^" << endl;
//					std::cerr << "else block name : " << elseName << endl;
					BasicBlock *bbElse = bi->getSuccessor(1);
					bbNames.clear();
					ki->falseBT = basicBlockOpGlobal(bbElse, elseNames, bbNames);
					std::string fullElse = getBlockFullName(bi, false);
//					std::set<std::string>::iterator elseIt = elseNames.begin(),
//							elseIe = elseNames.end();
//					for (; elseIt != elseIe; elseIt++) {
//						std::cerr << "else global var: " << *elseIt << endl;
//					}
//					std::cerr << "false full name : " << fullElse << endl;
//					std::cerr << "ki->falseBT : " << ki->falseBT << endl;
//					std::cerr << "=========================" << endl;
					if (ki->falseBT == KInstruction::definite) {
						rdManager.bbOpGVarName.insert(make_pair(bbElse, elseNames));
						rdManager.ifBB.insert(make_pair(fullElse, bbElse));
					}
				}
			}
		}
		}
	}
//	getMatchingPair(executor);
	getMPFromBlockPair(executor);
}

bool ListenerService::belongToRuntimeDir(const std::string &fileName) {
	unsigned len = fileName.size();
//	std::cerr << "len = " << len <<  endl;
	int left = -1;
	int slashNum = 0;
	std::string name= "";

	for (int i = len - 1; i >= 0; i--) {
		if (fileName[i] == '/') {
			slashNum++;
			if (slashNum == 3) {
				left = i;
				break;
			}
		}
	}

//	std::cerr << "left = " << left << ", " << fileName[left] << endl;
	if (left == -1)
		return false;
	else {
		for (int i = left + 1; fileName[i] != '/'; i++) {
			name += fileName[i];
//			std::cerr << "file name i " << fileName[i] << endl;
		}
	}

//	std::cerr << "name = " << name << endl;
	if (name == "runtime")
		return true;
	else
		return false;
}

void ListenerService::getMPFromBlockPair(Executor *executor) {
	std::map<BasicBlock *, std::set<string> >::iterator mit = rdManager.bbOpGVarName.begin(),
			mie = rdManager.bbOpGVarName.end();
	unsigned cnt = 0;

	for (; mit != mie; mit++, cnt++) {
		unsigned delta = 0;
		std::set<std::string> nameSet1 = mit->second;
		std::map<BasicBlock *, std::set<string> >::iterator innerIt = rdManager.bbOpGVarName.begin(),
				innerIe = rdManager.bbOpGVarName.end();
		while (delta <= cnt) {
			innerIt++;
			delta++;
		}
		std::string branchName1 = mit->first->getParent()->getName().str() +
				"." + mit->first->getName().str();

		for (; innerIt != innerIe; innerIt++) {
			if (mit->first->getParent()->getName().str() ==
					innerIt->first->getParent()->getName().str())
				continue;

			std::set<std::string> nameSet2 = innerIt->second;
			std::string branchName2 = innerIt->first->getParent()->getName().str() +
					"." + innerIt->first->getName().str();

			unsigned vecSize = nameSet1.size() > nameSet2.size() ? nameSet1.size() : nameSet2.size();
			std::set<std::string>::iterator firstIt = nameSet1.begin(), firstIe = nameSet1.end();
			std::set<std::string>::iterator secondIt = nameSet2.begin(), secondIe = nameSet2.end();
			std::vector<std::string> intersectRet(vecSize);
			std::vector<std::string>::iterator vecIt;

			vecIt = std::set_intersection(firstIt, firstIe, secondIt, secondIe, intersectRet.begin());

			intersectRet.resize(vecIt - intersectRet.begin());

			if (intersectRet.size() > 0) {
				// a matching pair found. store branches of the two br statement.
				// like pair<if.then, if.then21> pair<if.then, if.else>
				rdManager.MP.insert(std::make_pair(branchName1, branchName2));
				rdManager.MP.insert(std::make_pair(branchName2, branchName1));

//				std::cerr << "MP first : " << branchName1 <<
//						", second : " << branchName2 << endl;
//				std::cerr << "MP size = " << rdManager.MP.size() << endl;
			}
		}
	}

	rdManager.allMP = rdManager.MP.size();
//	rdManager.allMP = tnpCnt;
//	std::multimap<std::string, std::string>::iterator mmit = rdManager.MP.begin(),
//			mmie = rdManager.MP.end();
//
//	for (; mmit != mmie; mmit++) {
//		std::cerr << mmit->first << " : " << mmit->second << endl;
//	}
//	std::cerr << "MP size = " << rdManager.MP.size() << endl;
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
ListenerService::basicBlockOpGlobal(
		llvm::BasicBlock *basicBlock,
		std::set<std::string> &allGvar,
		std::set<std::string> &bbNames) {
	KInstruction::BranchType ret = KInstruction::none;
	std::string name = basicBlock->getName().str();
	bbNames.insert(name);
	for (BasicBlock::iterator bit = basicBlock->begin(), bie = basicBlock->end();
			bit != bie; bit++) {
//		bit->dump();
		if (bit->getOpcode() == Instruction::Load) {
			LoadInst *li = dyn_cast<LoadInst>(bit);
			std::string varName = bit->getOperand(0)->getName().str();
//			std::cerr << "Load name : " << varName << endl;

			if (li->getType()->getTypeID() == Type::PointerTyID) {
//				std::cerr << "basic block name: " << basicBlock->getName().str() << endl;
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
		}
		else if (bit->getOpcode() == Instruction::Br) {
				 BranchInst *bi = cast<BranchInst>(bit);
			if (bi->isUnconditional()) {
				std::string succName = bi->getSuccessor(0)->getName().str();
				std::string dotName;
				int t = -1;
				for (unsigned i = 0; i < succName.size(); i++) {
					if (succName[i] == '.') {
						t = i;
					}
				}
				if (t != -1) {
				t++;
				dotName = succName.substr(t);
				}
				if (bbNames.find(succName) != bbNames.end()) {
						continue;
				} else {
					if (t != -1) {
						if (strncmp(dotName.c_str(), "end", 3) == 0) {
//								std::cerr << "end1 : " << succName << endl;
								continue;
						}
					}
				}

				klee::KInstruction::BranchType retTemp =
						basicBlockOpGlobal(bi->getSuccessor(0), allGvar, bbNames);
				if (retTemp == klee::KInstruction::possible) {
					return klee::KInstruction::possible;
				}
				if (retTemp == klee::KInstruction::definite)
					ret = klee::KInstruction::definite;

			} else {
				std::string name1 = bi->getSuccessor(0)->getName().str();
				std::string name2 = bi->getSuccessor(1)->getName().str();
				std::string dotName;
				int t = -1;
				for (unsigned i = 0; i < name2.size(); i++) {
					if (name2[i] == '.') {
						t = i;
					}
				}
				if (t != -1) {
				t++;
					dotName = name2.substr(t);
				}
				klee::KInstruction::BranchType ret1 = klee::KInstruction::none;
				if (bbNames.find(name1) == bbNames.end())
					ret1 = basicBlockOpGlobal(bi->getSuccessor(0), allGvar, bbNames);
				klee::KInstruction::BranchType ret2 = klee::KInstruction::none;
				if (bbNames.find(name2) == bbNames.end()) {
					if (t != -1) {
						if (!(strncmp(dotName.c_str(), "end", 3) == 0)) {
//							std::cerr << "end2 : " << name2 << ", dotName : " << dotName << endl;
							ret2 = basicBlockOpGlobal(bi->getSuccessor(1), allGvar, bbNames);
						}
					}
				}

				if (ret1 == klee::KInstruction::possible ||
						ret2 == klee::KInstruction::possible) {
					return klee::KInstruction::possible;
				}

				if (ret1 == klee::KInstruction::definite ||
						ret2 == klee::KInstruction::definite)
					ret = klee::KInstruction::definite;
			}
		}else if (bit->getOpcode() == Instruction::Call) {
			CallInst* ci = cast<CallInst>(bit);
			unsigned argvs = ci->getNumArgOperands();
			Function* func = ci->getCalledFunction();

			if (func->getName().str() == "printf" ||
					func->getName().str() == "sprintf" ||
					func->getName().str() == "fprintf") {
				continue;
			}
			for (unsigned i = 0; i < argvs; i++) {
				if (ci->getOperand(i)->getType()->getTypeID() == Type::PointerTyID) {
					return klee::KInstruction::possible;
				}
			}


			std::set<std::string> funcNameSet;

			if (!func->isDeclaration()) {
				std::set<std::string> opGVar;
				klee::KInstruction::BranchType temp = funcOpGlobal(funcNameSet, func, opGVar);
				if (temp == klee::KInstruction::possible) {
					return klee::KInstruction::possible;
				}
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
				if (funcNameSet.size() > 0 &&
						funcNameSet.find(func->getName().str()) != funcNameSet.end())
					continue;
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
