/***
 * InputGenListener.cpp
 *
 * created on: 2016.2.28
 * 	Author: LIU Pei
 */

#include "InputGenListener.h"
#include "BitcodeListener.h"
#include "SymbolicListener.h"
#include "llvm/IR/Type.h"
#include "KQuery2Z3.h"
#include <iostream>
#include <z3++.h>
#include <vector>
#include <map>

using namespace llvm;

#define BinaryDebug 0

namespace klee {

InputGenListener::InputGenListener(Executor *executor, RuntimeDataManager * rdManager):
		BitcodeListener(), executor(executor), rdManager(rdManager), z3_solver(z3_ctx) {
}

InputGenListener::~InputGenListener() {

}

void InputGenListener::beforeRunMethodAsMain(ExecutionState &state) {
	Trace * trace = rdManager->getCurrentTrace();
	currentEvent = trace->path.begin();
	endEvent = trace->path.end();
}

void InputGenListener::executeInstruction(ExecutionState &state, KInstruction *ki) {
	if ((*currentEvent)) {
		Instruction * inst = ki->inst;
		Thread * thread = state.currentThread;
		switch (inst->getOpcode()) {
		case Instruction::Br: {
#if BinaryDebug
			std::cerr << "begin Br.\n";
#endif
			BranchInst * bi = dyn_cast<BranchInst>(inst);
			if (!bi->isUnconditional()) {
				ref<Expr> value = executor->eval(ki, 0, thread).value;
				//in order to execute successfully, replace the
				//symbolic expr with the first pass concrete value.
				//for execute to the right branch.
				if (value->getKind() != Expr::Constant) {
					Expr::Width width = value->getWidth();
					ref<Expr> concreteValue;
					if ((*currentEvent)->condition == true) {
						concreteValue = ConstantExpr::create(true, width);
					} else {
						concreteValue = ConstantExpr::create(false, width);
					}
					std::cerr << "branch value is " ;
					concreteValue->dump();
					ref<Expr> constraint = EqExpr::create(value, concreteValue);
					Executor::BinTree * brNode = new Executor::binTree();
					brNode->vecExpr.push_back(value);
					brNode->size = width;
					if (executor->headSentinel == NULL) {
						executor->headSentinel = brNode;
						executor->currTreeNode = brNode;
					} else {
						Executor::BinTree * tmp = executor->currTreeNode;
						tmp->next = brNode;
						executor->currTreeNode = brNode;
					}
					std::cerr << "print constraint of br related ";
					constraint->dump();
					executor->evalAgainst(ki, 0, thread, concreteValue);
				}
			}
#if BinaryDebug
			std::cerr << "end of Br\n";
#endif
			break;
		}
		case Instruction::Switch: {
#if BinaryDebug
			std::cerr << "begin Switch\n";
#endif
			//the same handling as branch statement.
			//replace symbolic value with concrete value.
			SwitchInst * si = cast<SwitchInst>(inst);
			ref<Expr> cond = executor->eval(ki, 0, thread).value;
			Expr::Width width = cond->getWidth();
			if (cond->getKind() != Expr::Constant) {
				Executor::BinTree * switchStat = new Executor::binTree();
				ref<Expr> concreteValue = (*currentEvent)->value.at(0);
				ref<Expr> mainCons = EqExpr::create(cond, concreteValue);
				switchStat->isSwitch = true;
				switchStat->size = width;
				for (SwitchInst::CaseIt it = si->case_begin(), ie = si->case_end();
						it != ie; it++) {
					ref<Expr> value = executor->evalConstant(it.getCaseValue());
					ref<Expr> constraint = EqExpr::create(cond, value);
					switchStat->vecExpr.push_back(constraint);

					std::cerr << "switch cond value = " << value << std::endl;
				}
				//insert the symbolic expression into the lingked list.
				if (executor->headSentinel == NULL) {
					executor->headSentinel = switchStat;
					executor->currTreeNode = switchStat;
				} else {
					Executor::BinTree * tmp = executor->currTreeNode;
					tmp->next = switchStat;
					executor->currTreeNode = switchStat;
				}
				std::cerr << "print the constraint in the switch statement\n";
				mainCons->dump();
				executor->evalAgainst(ki, 0, thread, concreteValue);
			}
#if BinaryDenbug
			std::cerr << "end of switch statement\n";
#endif
			break;
		}
		}
	}
}

void InputGenListener::instructionExecuted(ExecutionState &state, KInstruction *ki) {
	if (currentEvent != endEvent)
		currentEvent++;
}

void InputGenListener::afterRunMethodAsMain() {
	std::cerr << "print the symbolic tree to debug.\n";

	if (executor->headSentinel != NULL) {
		Executor::BinTree * head = executor->headSentinel;
		printSymbolicNode(head);
	}

	std::cerr << "input generate calling start.\n";
	inputGen(InputGenListener::DFS);
}

void InputGenListener::printSymbolicNode(Executor::BinTree * node) {
	if (node == NULL)
		return ;
	printSymbolicNode(node->next);
	std::vector<ref<Expr> >::iterator it =
			node->vecExpr.begin(), ie = node->vecExpr.end();
	for (; it != ie; it++) {
		(*it)->dump();
	}
}

void InputGenListener::createThread(ExecutionState &state, Thread *threa) {
}

void InputGenListener::executionFailed(ExecutionState &state, KInstruction *ki) {
	rdManager->getCurrentTrace()->traceType = Trace::FAILED;
}

void InputGenListener::inputGen(SearchType type) {
	switch (type) {
	case DFS: {
		//DFS search
		std::vector<ref<Expr> > constraints;
		Executor::BinTree * head = executor->headSentinel;
		DFSGenInput(head, constraints);
		break;
	}
	case BFS: {
		//BFS search
		Executor::BinTree * head = executor->headSentinel;
		BFSGenInput(head);
		break;
	}
	default: {
		break;
	}
	}
}

void InputGenListener::DFSGenInput(Executor::BinTree * head, std::vector<ref<Expr> > &constraints) {
	if (head == NULL) {
		std::cerr << "DFSGenInput\n";
		KQuery2Z3 * kq = new KQuery2Z3(z3_ctx);
		z3_solver.push();
		std::vector<ref<Expr> >::iterator it =
				constraints.begin(), ie = constraints.end();
		while (it != ie) {
			(*it)->dump();
			z3::expr res = kq->getZ3Expr((*it));
			std::cerr << "res = " << res << std::endl;
			z3_solver.add(res);
			it++;
		}
		check_result result = z3_solver.check();
		if (result == z3::sat) {
			model m = z3_solver.get_model();
			std::cerr << m << std::endl;
			//need complement in order to get the corresponding concrete value.
		}
		return;
	}
	if (!head->isSwitch) {
		for (int i = 0; i < 2; i++) {
			ref<Expr> lhs;
			std::vector<ref<Expr> >::size_type size = head->vecExpr.size();
			assert(size == 1 && "size of the vector greater than 1");
			ref<Expr> rhs = head->vecExpr[0];
			if (i == 0) {
				lhs = ConstantExpr::create(true, head->size);
			} else {
				lhs = ConstantExpr::create(false, head->size);
			}
			ref<Expr> constraint = EqExpr::create(lhs, rhs);
			constraints.push_back(constraint);
			DFSGenInput(head->next, constraints);
			constraints.pop_back();
		}
	} else {
		//special handle on switch statement.
		std::vector<ref<Expr> >::iterator it =
				head->vecExpr.begin(), ie = head->vecExpr.end();
		for (; it != ie; it++) {
			constraints.push_back((*it));
			DFSGenInput(head->next, constraints);
			constraints.pop_back();
		}
	}
}


void InputGenListener::BFSGenInput(Executor::BinTree * head) {

}

ref<Expr> InputGenListener::readExpr(ExecutionState &state, ref<Expr> address,
		Expr::Width size) {
	ObjectPair op;
	executor->getMemoryObject(op, state, address);
	const MemoryObject *mo = op.first;
	ref<Expr> offset = mo->getOffsetExpr(address);
	const ObjectState *os = op.second;
	ref<Expr> result = os->read(offset, size);
	return result;
}

}
