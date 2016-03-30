/*****
 * InputGenListener.h implemented collecting all branches
 *
 */

#ifndef _INPUT_GEN_LISTENER_H_
#define _INPUT_GEN_LISTENER_H_

#include "Executor.h"
#include "RuntimeDataManager.h"
#include "BitcodeListener.h"
#include "klee/ExecutionState.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/IntrinsicInst.h"
#include "DealWithSymbolicExpr.h"
#include "Event.h"
#include "Encode.h"

namespace klee {

class InputGenListener: public BitcodeListener {
public:
	InputGenListener(Executor *, RuntimeDataManager *);
	~InputGenListener();

	typedef enum {
		DFS,
		BFS
	}SearchType;
	void beforeRunMethodAsMain(ExecutionState &initialState);
	void executeInstruction(ExecutionState &state, KInstruction *ki);
	void instructionExecuted(ExecutionState &state, KInstruction *ki);
	void afterRunMethodAsMain();
//	void createMutex(ExecutionState &state, Mutex* mutex);
//	void createCondition(ExecutionState &state, Condition* condition);
	void createThread(ExecutionState &state, Thread* thread);
	void executionFailed(ExecutionState &state, KInstruction *ki);

	ref<Expr> manualMakeSymbolic(ExecutionState& state,
			std::string name, unsigned size, bool isFloat);

	void inputGen(SearchType searchType);
	void makeBasicArgvConstraint(
			std::vector<ref<Expr> > &constraints);

	void getArgvResult();
	void insertInputPrefix(Encode& encode,
			std::map<std::string, char>& charInfo, Event* event);
	void getPrefixFromPath(std::vector<Event*>&, Event*);

	void negateBranchForDefUse(Executor::BinTree *);
	void deleteMPFromCurrExe(std::string, Executor::BinTree*);
	bool processFirstBr(std::string, Executor::BinTree *);
	void processSecondBr(std::string, Executor::BinTree *);
	void negateThisBranch(Executor::BinTree *);

private:
	Executor *executor;
	RuntimeDataManager * rdManager;
	DealWithSymbolicExpr filter;
	std::vector<Event*>::iterator currentEvent, endEvent;
	context z3_ctx;
	solver z3_solver;


	void printSymbolicNode(Executor::BinTree *);
	ref<Expr> readExpr(ExecutionState &state, ref<Expr> address,
			Expr::Width size);

	void DFSGenInput(Executor::BinTree *, std::vector<ref<Expr> >&);
	void BFSGenInput(Executor::BinTree *);
	void negateEachBr(Executor::BinTree*, std::vector<ref<Expr> >&);
	void getSolveResult(std::vector<ref<Expr> >&, Executor::BinTree*);
	void freeMemoryBinTree(Executor::BinTree*);

};

}

#endif /* the end of the InputGenListener.h */
