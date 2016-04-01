/*
 * ListenerService.h
 *
 *  Created on: 2015年11月13日
 *      Author: zhy
 */

#ifndef LIB_CORE_LISTENERSERVICE_H_
#define LIB_CORE_LISTENERSERVICE_H_

#include <vector>

#include "klee/ExecutionState.h"
#include "BitcodeListener.h"
#include "RuntimeDataManager.h"

namespace klee {

class ListenerService {

private:
	std::vector<BitcodeListener*> bitcodeListeners;
	RuntimeDataManager rdManager;
	unsigned runState;
	struct timeval start, finish;

	klee::KInstruction::BranchType
		basicBlockOpGlobal(llvm::BasicBlock *basicBlock, std::set<std::string> &);
	klee::KInstruction::BranchType
		funcOpGlobal(std::set<std::string>&, llvm::Function *, std::set<std::string>&);
	klee::KInstruction::BranchType instOpGlobal(llvm::Instruction *, std::set<std::string> &);
	std::string getBlockFullName(llvm::BranchInst *, bool brCond);
	klee::KInstruction::BranchType processEntryBlock(llvm::BasicBlock *);

	bool searchBasicBlock(llvm::BasicBlock *basicBlock);
	bool searchFuncion(std::set<std::string> &, llvm::Function *function);


public:
	ListenerService(Executor* executor);
	~ListenerService(){}
	void pushListener(BitcodeListener* bitcodeListener);
	void removeListener(BitcodeListener* bitcodeListener);
	void popListener();

	RuntimeDataManager* getRuntimeDataManager();

	void preparation(Executor* executor);
	void getMatchingPair(Executor *executor);
	void getMPFromBlockPair(Executor *executor);
	void beforeRunMethodAsMain(ExecutionState &initialState);
	void executeInstruction(ExecutionState &state, KInstruction *ki);
	void instructionExecuted(ExecutionState &state, KInstruction *ki);
	void afterRunMethodAsMain();
	void executionFailed(ExecutionState &state, KInstruction *ki);

//	void createMutex(ExecutionState &state, Mutex* mutex);
//	void createCondition(ExecutionState &state, Condition* condition);
	void createThread(ExecutionState &state, Thread* thread);

	void startControl(Executor* executor);
	void endControl(Executor* executor);

	void changeInputAndPrefix(int argc, char **argv, Executor* executor);

	void markBrOpGloabl(Executor *executor);

	std::set<std::string> globalVarNameSet;
};

}

#endif /* LIB_CORE_LISTENERSERVICE_H_ */
