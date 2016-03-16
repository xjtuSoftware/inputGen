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

	bool basicBlockOpGlobal(llvm::BasicBlock *basicBlock);
	bool funcOpGlobal(std::set<std::string>&, llvm::Function *);


public:
	ListenerService(Executor* executor);
	~ListenerService(){}
	void pushListener(BitcodeListener* bitcodeListener);
	void removeListener(BitcodeListener* bitcodeListener);
	void popListener();

	RuntimeDataManager* getRuntimeDataManager();

	void preparation(Executor* executor);
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

	std::set<std::string> globalVarNameSet;
};

}

#endif /* LIB_CORE_LISTENERSERVICE_H_ */
