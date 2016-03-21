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
#define MakeSymbolic 0
#define BIT_WIDTH 64

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
			//get rid of br instruction from while and for.
			BranchInst * bi = dyn_cast<BranchInst>(inst);
			if (bi->isUnconditional()) {
				break;
			}
			ref<Expr> value = executor->eval(ki, 0, thread).value;
			Expr::Width width = value->getWidth();
//			std::cerr << "width = " << width << std::endl;
			ref<Expr> concreteValue;
			if (value->getKind() != Expr::Constant) {
				if ((*currentEvent)->condition == true) {
					concreteValue = ConstantExpr::create(true, width);
				} else {
					concreteValue = ConstantExpr::create(false, width);
				}
				executor->evalAgainst(ki, 0, thread, concreteValue);
			}

			std::string bbName = inst->getParent()->getName().str();
			if (bbName[0] == 'w') {
				if (strcmp(bbName.c_str(), "while.cond") == 0 ||
						strcmp(bbName.c_str(), "while.body") == 0) {
					break;
				}
			}
			if (bbName[0] == 'f') {
				if (strcmp(bbName.c_str(), "for.cond") == 0 ||
						strcmp(bbName.c_str(), "for.body") == 0 ||
						strcmp(bbName.c_str(), "for.inc")) {
					break;
				}
			}
			if ((*currentEvent)->condition) {
				if (ki->falseBlockHasGlobal) {
					std::cerr << "line number : " << ki->info->line << std::endl;
					std::cerr << "we can negate the false parts.\n";
				}
			} else {
				if (ki->trueBlockHasGlobal) {
					std::cerr << "line number : " << ki->info->line << std::endl;
					std::cerr << "we can negate the true parts.\n";
				}
			}
			if (!bi->isUnconditional()) {
				//in order to execute successfully, replace the
				//symbolic expr with the first pass concrete value.
				//for execute to the right branch.
//					ref<Expr> constraint = EqExpr::create(value, concreteValue);
					Executor::BinTree * brNode = new Executor::binTree();

					brNode->vecExpr.push_back(value);
					brNode->size = width;
					brNode->brTrue = (*currentEvent)->condition;
					brNode->currEvent = (*currentEvent);
					if (executor->headSentinel == NULL) {
						executor->headSentinel = brNode;
						executor->currTreeNode = brNode;

					} else {
						Executor::BinTree * tmp = executor->currTreeNode;
						tmp->next = brNode;
						executor->currTreeNode = brNode;

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
			cond->dump();
			Expr::Width width = cond->getWidth();
			if (cond->getKind() != Expr::Constant) {
				Executor::BinTree * switchStat = new Executor::binTree();
				ref<Expr> concreteValue = (*currentEvent)->value.at(0);
				std::cerr << "concrete value : " << concreteValue << std::endl;
				ref<Expr> mainCons = EqExpr::create(cond, concreteValue);
				switchStat->isSwitch = true;
				switchStat->switchValue = concreteValue;
				switchStat->currEvent = (*currentEvent);
				switchStat->size = width;
				for (SwitchInst::CaseIt it = si->case_begin(), ie = si->case_end();
						it != ie; it++) {
					ref<Expr> value = executor->evalConstant(it.getCaseValue());
					if (value.compare(concreteValue) != 0) {
						ref<Expr> constraint = EqExpr::create(cond, value);
						switchStat->vecExpr.push_back(constraint);
						std::cerr << "switch cond value = " << value << std::endl;
					}

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
		case Instruction::Call: {
			//in order to prevent down from symbolic arguments
			//in function printf, puts and so on.
			if (!(*currentEvent)->isFunctionWithSourceCode) {
				CallSite cs(inst);
				Function* f = (*currentEvent)->calledFunction;
				if (f->getName() == "printf" || f->getName() == "puts") {
					unsigned numArgs = cs.arg_size();
					for (unsigned i = 1; i <= numArgs; i++) {
						ref<Expr> value = executor->eval(ki, i, thread).value;
						if (value->getKind() != Expr::Constant) {
							ref<Expr> concreteValue = (*currentEvent)->value[i - 1];
							std::cerr << "concrete value : " << concreteValue << std::endl;
							executor->evalAgainst(ki, i, thread, concreteValue);
						}
					}
				}
			}
			break;
		}
		}
	}
}

void InputGenListener::instructionExecuted(ExecutionState &state, KInstruction *ki) {
	//handle call instruction of functions atoi, strlen and so on.
	if ((*currentEvent)) {
		Instruction* inst = ki->inst;
//		inst->dump();
		Thread* thread = state.currentThread;
		switch (inst->getOpcode()) {
		case Instruction::Call: {
			if (!(*currentEvent)->isFunctionWithSourceCode) {
				CallSite cs(inst);
				Function* f = (*currentEvent)->calledFunction;
				if (f->getName() == "atoi") {
					//get the read result
					ref<Expr> address = executor->eval(ki, 1, thread).value;
					ObjectPair op;
					bool success = executor->getMemoryObject(op, state, address);
					if (success) {
						const ObjectState* os = op.second;
						ref<Expr> offset = op.first->getOffsetExpr(address);
						Expr::Width chWidth = 8;
						ref<Expr> value = os->read(offset, chWidth);
						if (value->getKind() != Expr::Constant) {
							//make atoi result symbolic.
							ref<Expr> retValue = executor->getDestCell(thread, ki).value;
							std::string retSymbol = "atoi";
							//get one char.
							ref<Expr> atoiExpr = manualMakeSymbolic(state, retSymbol, 8, false);
							executor->setDestCell(thread, ki, atoiExpr);
						}
					} else {
						assert(0 && "function atoi has problems in get ObjectState.\n");
					}
				} else if (f->getName() == "strlen") {
					ref<Expr> concreteValue = (*currentEvent)->value[1];
					executor->setDestCell(thread, ki, concreteValue);
					// we can implement to enumerate the length of the string.
					if (concreteValue->getKind() != Expr::Constant) {
						assert(0 && "strlen return value is not a constant.\n");
					}
					if (ConstantExpr* ce = dyn_cast<ConstantExpr>(concreteValue)) {
						unsigned value = ce->getZExtValue();
						std::cerr << "the value of strlen is " << value << std::endl;
					}
				} else if (f->getName() == "implAtoI"){
//					std::cerr << "implAtoI : " << inst->getNumOperands() << std::endl;
					unsigned numArgs = inst->getNumOperands();
					for (unsigned i = 0; i < (numArgs - 1); i++) {
						std::string varName = inst->getOperand(i)->getName().str();
						ref<Expr> address = executor->eval(ki, i + 1, thread).value;
						Expr::Width width = executor->getWidthForLLVMType(inst->getOperand(i)->getType());

						//8 bits. the problems in here. 坑
						ref<Expr> retSym = manualMakeSymbolic(state, varName, sizeof(int) * 8/*define as char*/, false);
						ObjectPair op;
						bool success = executor->getMemoryObject(op, state, address);
						if (success) {
							const ObjectState *os = op.second;
							const MemoryObject *mo = op.first;
							ObjectState *wos = state.addressSpace.getWriteable(mo, os);
							ref<Expr> offset = mo->getOffsetExpr(address);
							wos->write(offset, retSym);
						} else {
							assert(0 && "can not get the corresponding op in InputGenListener.\n");
						}
//						executor->eval(ki, i + 1, thread).value->dump();
						executor->intArgvConstraints.insert(retSym);

						rdManager->intArgv.insert(make_pair(varName, -1));

//						std::cerr << "name : " << inst->getOperand(i)->getName().str() << "\n";
					}
//					std::cerr << "print end.\n";
				}
			}
			break;
		}
		}
	}
	#if MakeSymbolic
	if (*currentEvent) {
		Instruction * inst = ki->inst;
		Thread * thread = state.currentThread;
		switch (inst->getOpcode()) {
		//handle load instruction and make symbolic
		case Instruction::Load: {
#if BinaryDebug
			std::cerr << "begin Load.\n";
#endif
			//symbolic value just like arg[number][number]
			ref<Expr> address = executor->eval(ki, 0, thread).value;
			if (ConstantExpr * ce = dyn_cast<ConstantExpr>(address)) {
				unsigned long long realAddress = ce->getZExtValue();
				int cnt = 0;
				std::map<unsigned long long, unsigned long long>::iterator it =
						executor->addressAndSize.begin(), ie = executor->addressAndSize.end();
				for (; it != ie; it++, cnt++) {
					if (realAddress >= it->first && realAddress <= it->second) {
						break;
					}
				}
				if (it != ie) {
					//make the address value symbolic
					ref<Expr> value = executor->getDestCell(thread, ki).value;
					if (ConstantExpr * vce = dyn_cast<ConstantExpr>(value)) {
						//make this value symbolic.
						//don't make '\0' symbolic.
						if (realAddress != it->second) {
							int pos = (realAddress - it->first) / (sizeof(char));
							std::stringstream ss;
							ss << "argv[" << cnt << "][" << pos  << "]";
							Expr::Width size = executor->getWidthForLLVMType(ki->inst->getType());
							ref<Expr> symbolic = manualMakeSymbolic(state, ss.str(), size, false);
							executor->setDestCell(thread, ki, symbolic);
							ss.clear();
						} else {
							//the case is the last char of the c-char-string '\0';
							//don't make this symbolic.
						}
					} else {
						//the value already symbolic
					}
				}
			} else {
				assert(0 && "the read address must be a ConstantExpr.\n");
			}
#if BinaryDebug
			std::cerr << "end Load.\n";
#endif
			break;
		}
		}
	}
#endif
	if (currentEvent != endEvent)
		currentEvent++;
}

void InputGenListener::afterRunMethodAsMain() {
	std::cerr << "print the symbolic tree to debug.\n";

	if (executor->headSentinel != NULL) {
		Executor::BinTree * head = executor->headSentinel;
		printSymbolicNode(head);
	}

//	std::cerr << "input generate calling start.\n";
	inputGen(InputGenListener::DFS);
//	if (rdManager->symbolicInputPrefix.size() == 0)
//		executor->setIsFinished();
}

void InputGenListener::printSymbolicNode(Executor::BinTree * node) {
	if (node == NULL)
		return ;
	std::vector<ref<Expr> >::iterator it =
			node->vecExpr.begin(), ie = node->vecExpr.end();
	for (; it != ie; it++) {
		(*it)->dump();
	}
	printSymbolicNode(node->next);

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
//		std::cerr << "start execute in DFS, constraints size = " << constraints.size() << std::endl;
		negateEachBr(head, constraints);
//		DFSGenInput(head, constraints);
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

void InputGenListener::freeMemoryBinTree(Executor::BinTree* head) {
	if (head) {
		freeMemoryBinTree(head->next);
		delete head->next;
		head->next = NULL;
	}
}

void InputGenListener::negateEachBr(Executor::BinTree* head,
		std::vector<ref<Expr> >& constraints) {
	makeBasicArgvConstraint(constraints);
	Executor::BinTree* temp = head;
	ref<Expr> pushValue = ConstantExpr::create(true, 1);
	while (temp != NULL) {
		if (temp->isSwitch) {
			//switch statement
			std::vector<ref<Expr> >::iterator it =
					temp->vecExpr.begin(), ie = temp->vecExpr.end();
			for (; it != ie; it++) {
				constraints.push_back(*it);
				getSolveResult(constraints, temp);
				constraints.pop_back();
			}
			pushValue = temp->switchValue;
		} else {
			//if statement.
			Expr::Width width = 1;
			ref<Expr> trueExpr = ConstantExpr::create(true, width);
			ref<Expr> falseExpr = ConstantExpr::create(false, width);
			ref<Expr> lhs = ConstantExpr::create(true, width);
			if (temp->brTrue) {
				pushValue = EqExpr::create(trueExpr, temp->vecExpr[0]);
				lhs = falseExpr;
			} else {
				pushValue = EqExpr::create(falseExpr, temp->vecExpr[0]);
				lhs = trueExpr;
			}
			ref<Expr> constraint = EqExpr::create(lhs, temp->vecExpr[0]);
			constraints.push_back(constraint);
//			std::cerr << "constraints size : " << constraints.size() << std::endl;
			getSolveResult(constraints, temp);
			constraints.pop_back();
		}
		constraints.push_back(pushValue);
		temp = temp->next;
	}
	//释放由于收集分支信息新建的BinTree
	freeMemoryBinTree(head);
	executor->headSentinel = NULL;
	executor->currTreeNode = NULL;
}

void InputGenListener::getSolveResult(std::vector<ref<Expr> >&
		constraints, Executor::BinTree* node) {
	KQuery2Z3 * kq = new KQuery2Z3(z3_ctx);
	std::vector<ref<Expr> >::iterator it =
			constraints.begin(), ie = constraints.end();
	//short for push.
	z3_solver.push();
	while (it != ie) {
//			(*it)->dump();
		z3::expr res = kq->getZ3Expr((*it));
		z3_solver.add(res);
		it++;
	}
	check_result result = z3_solver.check();
	if (result == z3::sat) {
		std::cerr << "satisfied the constraints in get solve result.\n";
		model m = z3_solver.get_model();
//		std::cerr << m << std::endl;
		//get every char.
		std::map<std::string, char>::iterator it =
				executor->charInfo.begin(), ie = executor->charInfo.end();
		std::stringstream sr;
		for (; it != ie; it++) {
			z3::expr charExpr = z3_ctx.bv_const(it->first.c_str(), BIT_WIDTH);
			z3::expr realExpr = z3::to_expr(z3_ctx, Z3_mk_bv2int(z3_ctx, charExpr, false));
			sr << m.eval(realExpr);
			int temp = atoi(sr.str().c_str());
			char ch = toascii(temp);
			std::cerr << "temp = " << temp << ", ch = " << ch << std::endl;
			executor->charInfo[it->first] = ch;
			sr.str("");
		}
		//compute int argvs.
		std::map<std::string, unsigned>::iterator iit = rdManager->intArgv.begin(),
				iie = rdManager->intArgv.end();
		for (; iit != iie; iit++) {
			z3::expr tempExpr = z3_ctx.bv_const(iit->first.c_str(), BIT_WIDTH);
			z3::expr realExpr = z3::to_expr(z3_ctx, Z3_mk_bv2int(z3_ctx, tempExpr, false));
			sr << m.eval(realExpr);
			std::cerr << "sr = " << sr.str().c_str() << std::endl;
			int temp = atoi(sr.str().c_str());
			std::cerr << "var = " << iit->first << ", value = " << temp << std::endl;
			rdManager->intArgv[iit->first] = (unsigned)temp;
			sr.str("");
		}
		std::cerr << "print the char.\n";
		std::map<std::string, char>::iterator mit =
				executor->charInfo.begin(), mie = executor->charInfo.end();
		for (; mit != mie; mit++) {
			std::cerr << mit->first << " : " << mit->second << std::endl;
		}
		//need complement in order to get the corresponding concrete value.
		std::vector<std::string> argvValue;
		std::map<std::string, char>::iterator mmit = executor->charInfo.begin(),
				mmie = executor->charInfo.end();
		if (mmit != mmie) {
		std::string tempStr = mmit->first;
		char argvStr[20] = "";
		int i = 0;
		argvStr[0] = '\0';
		argvStr[i++] = mmit->second;
		mmit++;
		for (; mmit != mmie; mmit++) {
			if (tempStr[5] == (mmit->first)[5]) {
				argvStr[i++] = mmit->second;
			} else {
				argvStr[i] = '\0';
				argvValue.push_back(std::string(argvStr));
				i = 0;
				tempStr = mmit->first;
				argvStr[i] = '\0';
				argvStr[i++] = mmit->second;
			}
		}
		argvStr[i] = '\0';
		argvValue.push_back(std::string(argvStr));
		}

		//get prefix and store them in the runtime data manager.
		std::vector<Event*> vecEvent;
		getPrefixFromPath(vecEvent, node->currEvent);
		Prefix* prefix = new Prefix(vecEvent,
				rdManager->getCurrentTrace()->createThreadPoint, "mapOfInputAndPreix");
		std::map<std::string, unsigned> tempMap;
		tempMap.insert(rdManager->intArgv.begin(), rdManager->intArgv.end());
		rdManager->symbolicInputPrefix.insert(make_pair(prefix, argvValue));
		rdManager->intInputPrefix.insert(make_pair(prefix, tempMap));
	} else if (result == z3::unsat) {
		std::cerr << "unsat inputGenListener.\n";
	} else {
		std::cerr << "unknown inputGenListener.\n";
	}
	z3_solver.pop();
	delete kq;
}

void InputGenListener::DFSGenInput(Executor::BinTree * head, std::vector<ref<Expr> > &constraints) {
	if (head == NULL) {
		makeBasicArgvConstraint(constraints);
		KQuery2Z3 * kq = new KQuery2Z3(z3_ctx);
		z3_solver.push();
		std::vector<ref<Expr> >::iterator it =
				constraints.begin(), ie = constraints.end();
		while (it != ie) {
//			(*it)->dump();
			z3::expr res = kq->getZ3Expr((*it));
			z3_solver.add(res);
			it++;
		}
		check_result result = z3_solver.check();
		if (result == z3::sat) {
			std::cerr << "satisfied by constraints.\n";
			model m = z3_solver.get_model();
			std::cerr << m << std::endl;
			//get every char.
			std::map<std::string, char>::iterator it =
					executor->charInfo.begin(), ie = executor->charInfo.end();
			std::stringstream sr;
			for (; it != ie; it++) {
				z3::expr charExpr = z3_ctx.bv_const(it->first.c_str(), BIT_WIDTH);
				z3::expr realExpr = z3::to_expr(z3_ctx, Z3_mk_bv2int(z3_ctx, charExpr, false));
				sr << m.eval(realExpr);
				std::cerr << "sr = " << sr.str().c_str() << std::endl;
				int temp = atoi(sr.str().c_str());
				char ch = toascii(temp);
				std::cerr << "temp = " << temp << ", ch = " << ch << std::endl;
				executor->charInfo[it->first] = ch;
				sr.str("");
			}
			//产生输入和对应的prefix
			std::cerr << "print the char.\n";
			std::map<std::string, char>::iterator mit =
					executor->charInfo.begin(), mie = executor->charInfo.end();
			for (; mit != mie; mit++) {
				std::cerr << mit->first << " : " << mit->second << std::endl;
			}
			//need complement in order to get the corresponding concrete value.
			std::vector<std::string> argvValue;
			std::map<std::string, char>::iterator mmit = executor->charInfo.begin(),
					mmie = executor->charInfo.end();
			std::string tempStr = mmit->first;
			char argvStr[20] = "";
			int i = 0;
			argvStr[0] = '\0';
			argvStr[i++] = mmit->second;
			mmit++;
			for (; mmit != mmie; mmit++) {
				if (tempStr[5] == (mmit->first)[5]) {
					argvStr[i++] = mmit->second;
				} else {
					argvStr[i] = '\0';
					argvValue.push_back(std::string(argvStr));
					i = 0;
					tempStr = mmit->first;
					argvStr[i] = '\0';
					argvStr[i++] = mmit->second;
				}
			}
			argvStr[i] = '\0';
			argvValue.push_back(std::string(argvStr));

			std::vector<std::string>::iterator vit = argvValue.begin(),
					vie = argvValue.end();
			for (; vit != vie; vit++) {
				std::cerr << "argv is " << (*vit) << std::endl;
			}
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

ref<Expr> InputGenListener::manualMakeSymbolic(ExecutionState& state,
		std::string name, unsigned size, bool isFloat) {

	//添加新的符号变量
	const Array *array = new Array(name, size, isFloat);
	ObjectState *os = new ObjectState(size, array);
	ref<Expr> offset = ConstantExpr::create(0, BIT_WIDTH);
	ref<Expr> result = os->read(offset, size);
	if (isFloat) {
		result.get()->isFloat = true;
	}
#if DEBUGSYMBOLIC
	cerr << "Event name : " << (*currentEvent)->eventName << "\n";
	cerr << "make symboic:" << name << std::endl;
	cerr << "is float:" << isFloat << std::endl;
	std::cerr << "result : ";
	result->dump();
	std::cerr << "symbolic result : " << result << std::endl;
#endif
	return result;
}


void InputGenListener::makeBasicArgvConstraint(
		std::vector<ref<Expr> > &constraints) {
	std::set<ref<Expr> >::iterator it = executor->argvSymbolics.begin(),
			ie = executor->argvSymbolics.end();
	for (; it != ie; it++) {
		//ASCII code upper and lower bound in decimal.
		ref<Expr> lowerBound = ConstantExpr::alloc(33, 8);
		ref<Expr> upperBound = ConstantExpr::alloc(126, 8);
		ref<Expr> lhs = UleExpr::create((*it), upperBound);
		ref<Expr> rhs = UgeExpr::create((*it), lowerBound);
		constraints.push_back(lhs);
		constraints.push_back(rhs);
	}

	std::set<ref<Expr> >::iterator sit = executor->intArgvConstraints.begin(),
			sie = executor->intArgvConstraints.end();
	for (; sit != sie; sit++) {
		ref<Expr> lowerBound = ConstantExpr::alloc(0, sizeof(int) * 8);
		ref<Expr> upperBound = ConstantExpr::alloc(255, sizeof(int) * 8);
		ref<Expr> lhs = UleExpr::create((*sit), upperBound);
		ref<Expr> rhs = UgeExpr::create((*sit), lowerBound);
		constraints.push_back(lhs);
		constraints.push_back(rhs);
	}
}

void InputGenListener::insertInputPrefix(Encode& encode,
		std::map<std::string, char>& charInfo, Event* event) {
//	encode.buildInputNeedFormula();
//	vector<Event*> vecEvent;
//	computePrefix(vecEvent, event);
//	Prefix* prefix = new Prefix(vecEvent, trace->createThreadPoint,
//			ss.str());
//	handle the map of charInfo. get the main input.
	std::vector<std::string> argvValue;
	std::map<std::string, char>::iterator it = charInfo.begin(),
			ie = charInfo.end();
	std::string tempStr = it->first;
	char argvStr[20];
	int i = 0;
	argvStr[0] = '\0';
	argvStr[i++] = it->second;
	it++;
	for (; it != ie; it++) {
		if (tempStr[5] == (it->first)[5]) {
			argvStr[i++] = it->second;
		} else {
			argvStr[i] = '\0';
			argvValue.push_back(std::string(argvStr));
			i = 0;
			tempStr = it->first;
			argvStr[i] = '\0';
			argvStr[i++] = it->second;
		}
	}
	argvValue.push_back(std::string(argvStr));

	std::vector<std::string>::iterator vit = argvValue.begin(),
			vie = argvValue.end();
	for (; it != ie; it++) {
		std::cerr << "argv is " << (*vit) << std::endl;
	}
}

void InputGenListener::getPrefixFromPath(
		std::vector<Event*>& vecEvent, Event* branchPoint) {
	Trace* trace = rdManager->getCurrentTrace();
	std::vector<Event*>::iterator it =
			trace->path.begin(), ie = trace->path.end();
	for (; it != ie; it++) {
		if ((*it) != branchPoint) {
			vecEvent.push_back((*it));
		} else {
			break;
		}
	}
}

}
