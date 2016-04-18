/*
 * DefUseBuilder.cpp
 *
 *  Created on: Apr 5, 2016
 *      Author: hhfan
 */

#include "DefUseBuilder.h"
#include "Executor.h"
#include "KQuery2Z3.h"
#include "llvm/IR/Module.h"
#include <map>
#include <vector>


using namespace std;
using namespace llvm;
using namespace z3;


#define DEBUG 0
#define PRINT_OPERATION_SET_DETAILED 0
#define PRINT_OPERATION_SET_BRIEFLY 0
#define PRINT_DEF_USE 0
#define DELETE_MP_DU 0

namespace klee {

DefUseBuilder::DefUseBuilder(RuntimeDataManager& data_t, Encode& encode_t) :
		rdManager(data_t), encode(encode_t) {
	trace = data_t.getCurrentTrace();
	std::cerr << "trace = " << trace << endl;
	currentEvent = NULL;
}

DefUseBuilder::~DefUseBuilder() {
	// TODO Auto-generated destructor stub
}

void DefUseBuilder::buildAllDefUse(){
//	std::cout << "build all def-use" << std::endl;
	std::cout << "<-->--<-->--<-->--<--><-->--<--><-->--<--><-->--<-->" << std::endl;
	buildDefUseForCurPath();
}

/*
 * when curRead->latestWrite == NULL, we don't know where we can read from(initialization or other thread).
 * so, we have to get the latestWrite of curRead in current path. Then reduce it.
 */
Event* DefUseBuilder::getLatestWriteInCurPath(Event* curRead, map<string, vector<Event *> >::iterator wsIte){
//	std::cout << "reduce marked type->1" << std::endl;
	Event* latestWrite = NULL;
	unsigned maxEventId = 0;
	for(unsigned i = 0; i < wsIte->second.size(); ++i){
		if(curRead->eventId > wsIte->second[i]->eventId && wsIte->second[i]->eventId > maxEventId){
			latestWrite = wsIte->second[i];
			maxEventId = latestWrite->eventId;
		}
	}
	return latestWrite;
}

/*
 * just consider shared memory access points.
 *
 * we have to reduce def-use in the following two situation:
 * (1)when curRead->latestWrite == curWrite, it's sure that the new def-use has been covered by current path.
 * 	  so, we reduce this kind of def-use in function buildDefUseForCurPath(don't construct such def-use).
 *
 * (2)when curRead->latestWrite == NULL, we don't know where we can read from(initialization or other thread).
 *    so, we have to get the latestWrite of curRead in current path. Then reduce it.
 *
 */
void DefUseBuilder::buildDefUseForCurPath() {

	std::cerr << "read set size:" << trace->allReadSet.size() << std::endl;
	std::map<std::string, std::vector<Event *> >::iterator itr = trace->allReadSet.begin(),
			ier = trace->allReadSet.end();
	unsigned cntR = 0;
	for (; itr != ier; itr++) {
		cntR += itr->second.size();
	}
	std::cerr << "size = " << cntR << endl;
	std::cout << "write set size:" << trace->allWriteSet.size() << std::endl;
	std::map<std::string, std::vector<Event *> >::iterator itw = trace->allWriteSet.begin(),
				iew = trace->allWriteSet.end();
		unsigned cntW = 0;
		for (; itw != iew; itw++) {
			cntW += itw->second.size();
		}
	std::cerr << "size = " << cntW << endl;

#if PRINT_OPERATION_SET_DETAILED
	std::cout << "print readSet(detailed):" << std::endl;
	printOpSet(trace->readSet);
	std::cout << "print writeSet(detailed):" << std::endl;
	printOpSet(trace->writeSet);
#endif

#if PRINT_OPERATION_SET_BRIEFLY
	std::cout << "print readSet(briefly):" << std::endl;
	printOpSetBriefly(trace->readSet);
	std::cout << "print writeSet(briefly):" << std::endl;
	printOpSetBriefly(trace->writeSet);
#endif

	printSingleThreadEvent();

	assert(trace->allReadSet.size() != 0 && "readSet is empty");
	markLatestWriteForGlobalVar();
//	reduceSet(trace->readSet);
//	reduceSet(trace->writeSet);

	Event* firPthCreEvent = getFirstPthreadCreateEvent();
	std::map<string, vector<Event *> >::iterator rsIte = trace->allReadSet.begin(); //key--variable
	for (; rsIte != trace->allReadSet.end(); ++rsIte) {
		map<string, vector<Event *> >::iterator wsIte = trace->allWriteSet.find(rsIte->first);
//		assert(iw != trace->writeSet.end());
		if(wsIte == trace->allWriteSet.end())
			continue;
		if(find(singleThreadEvent.begin(), singleThreadEvent.end(), rsIte->first) != singleThreadEvent.end())
			continue;

		/*
		 * Tid = 1的线程，在未创建线程之前，其线程内读写操作所构造出的def-use chain是不可能触发程序bug的。
		 * 因此，对于上述def-use pair不予构建，以约减之
		 */
		for (unsigned k = 0; k < rsIte->second.size(); ++k) {
			if(rsIte->second[k]->eventId < firPthCreEvent->eventId)
				continue;

			Event *curRead;
			Event *curWrite;
			curRead = rsIte->second[k];
			if (curRead->latestWrite == NULL) {
			/*
			 * do not exist latest write operation in the same thread.
			 * maybe read from initialization or other thread.
			 */
				//read from initialization
				curWrite = NULL;
				if(curWrite == getLatestWriteInCurPath(curRead, wsIte)){
					buildDefUse(curRead, curWrite, wsIte);
				} else{
					buildAndVerifyDefUse(curRead, curWrite, wsIte);;
				}

				//read from other thread
				for(unsigned l = 0; l < wsIte->second.size(); ++l){
					if(curRead->threadId == wsIte->second[l]->threadId)
						continue;
					curWrite = wsIte->second[l];
					if(curWrite == getLatestWriteInCurPath(curRead, wsIte)){
						buildDefUse(curRead, curWrite, wsIte);
					} else{
						buildAndVerifyDefUse(curRead, curWrite, wsIte);
					}
				}
			} else {
			/*
			 * exist latest write operation in the same thread.
			 * read from latest write(same thread) or other thread
			 */
				for (unsigned l = 0; l < wsIte->second.size(); ++l) {
					curWrite = wsIte->second[l];
					if(curWrite == getLatestWriteInCurPath(curRead, wsIte)){	//explicit def-use
						buildDefUse(curRead, curWrite, wsIte);
					} else{ 													//implicit def-use
						if(curRead->threadId == curWrite->threadId){ 			///read from the same thread
							if(curRead->latestWrite == curWrite){
								buildAndVerifyDefUse(curRead, curWrite, wsIte); ///read from latestWrite
							}
						} else{ 												///read from other threads
							buildAndVerifyDefUse(curRead, curWrite, wsIte);
						}
					}
				}
			}
		}
	}
}

void DefUseBuilder::buildAndVerifyDefUse(Event* curRead, Event* curWrite,
								map<string, vector<Event *> >::iterator iw){
	DefUse* def_use = new DefUse;
	def_use->pre = curWrite;
	def_use->post = curRead;
	if(!isCoveredPrePath(def_use)){
		expr defUseExpr = encode.z3_ctx.int_const("E_INIT");
		buildExpr(curRead, curWrite, defUseExpr);
		if(isValid(curRead, curWrite, defUseExpr)) {
			rdManager.coveredDefUse_pre.push_back(def_use);
			rdManager.implicitDefUse_pre.push_back(def_use);
			removeFromUnsolved(def_use);
#if DELETE_MP_DU
			if (curWrite != NULL) {
				std::string bbName1 = curWrite->inst->inst->getParent()->getParent()->getName().str() + "." +
						curWrite->inst->inst->getParent()->getName().str();
				std::string bbName2 = curRead->inst->inst->getParent()->getParent()->getName().str() + "." +
						curRead->inst->inst->getParent()->getName().str();

				for (std::multimap<std::string, std::string>::iterator it = rdManager.MP.begin(),
						ie = rdManager.MP.end(); it != ie; it++) {
					if (it->first == bbName1 && it->second == bbName2) {
						rdManager.MP.erase(it);
					}
				}
			}
#endif
			std::cerr << "the constraint can be solved!" << std::endl;
			return ;
		} else {
			if(!isUnsolvedPrePath(def_use)){
				rdManager.unsolvedDefUse_pre.push_back(def_use);
				return ;
			}
		}
	}
	delete def_use;
	return ;
}

void DefUseBuilder::buildDefUse(Event* curRead, Event* curWrite,
								map<string, vector<Event *> >::iterator iw){
	DefUse* def_use = new DefUse;
	def_use->pre = curWrite;
	def_use->post = curRead;
	if(!isCoveredExplicit(def_use)){
		rdManager.explicitDefUse_pre.push_back(def_use);
#if DELETE_MP_DU
		if (curWrite != NULL) {
			std::string bbName1 = curWrite->inst->inst->getParent()->getParent()->getName().str() + "." +
					curWrite->inst->inst->getParent()->getName().str();
			std::string bbName2 = curRead->inst->inst->getParent()->getParent()->getName().str() + "." +
					curRead->inst->inst->getParent()->getName().str();

			for (std::multimap<std::string, std::string>::iterator it = rdManager.MP.begin(),
					ie = rdManager.MP.end(); it != ie; it++) {
				if (it->first == bbName1 && it->second == bbName2) {
					rdManager.MP.erase(it);
				}
			}
		}
#endif
//		std::cout << "just build" << std::endl;
		if(!isCoveredPrePath(def_use)){
			rdManager.coveredDefUse_pre.push_back(def_use);
			return ;
		}
		return ;
	}
	delete def_use;
	return ;
}


void DefUseBuilder::buildExpr(Event* curRead, Event* curWrite,
								expr& defUseExpr) {

	if(curWrite == NULL){ //read from initialization
		expr curReadExpr = encode.z3_ctx.int_const(curRead->eventName.c_str());
		defUseExpr = (defUseExpr < curReadExpr);

#if PRINT_DEF_USE
		std::cout << "def_use_order_type1" << std::endl;
		std::cout << def_use_order_tpye1 << std::endl;
		std::cout << currentRead->toString() << std::endl;
#endif

	} else{					//read from other thread
		expr curWriteExpr = encode.z3_ctx.int_const(curWrite->eventName.c_str());
		expr curReadExpr = encode.z3_ctx.int_const(curRead->eventName.c_str());

		Event*  curWriteNext = getNextEventInThread(curWrite);
		Event* curReadPrev = getPrevEventInThread(curRead);
		expr curWriteNextExpr = encode.z3_ctx.int_const(curWriteNext->eventName.c_str());
		expr curReadPrevExpr = encode.z3_ctx.int_const(curReadPrev->eventName.c_str());

		assert((curWriteNext != curWrite || curReadPrev != curRead) && "trace error!");
		if(curWriteNext != curWrite && curReadPrev != curRead){
			if(curWriteNext == NULL){
				if(curReadPrev == NULL){
					defUseExpr = (curWriteExpr < curReadExpr);
				} else{
					defUseExpr = (curWriteExpr < curReadExpr) &&
									(curReadPrevExpr < curWriteExpr);
				}
			} else {
				if(curRead == NULL){
					defUseExpr = (curWriteExpr < curReadExpr)&&
									(curReadExpr < curWriteNextExpr);
				} else{
					defUseExpr = (curWriteExpr < curReadExpr)&&
									(curReadPrevExpr < curWriteExpr) &&
									(curReadExpr < curWriteNextExpr);
				}
			}
		}

#if PRINT_DEF_USE
		std::cout << "def_use_order_type2" << std::endl;
		std::cout << def_use_order_type2 << std::endl;
		std::cout << currentRead->toString() << std::endl;
		std::cout << currentWrite->toString() << std::endl;
#endif

	}

}


void DefUseBuilder::buildExpr_AddAllWrite(Event* curRead, Event* curWrite,
								map<string, vector<Event *> >::iterator iw,
								expr& defUseExpr) {

	if(curWrite == NULL){ //read from initialization
		expr curReadExpr = encode.z3_ctx.int_const(curRead->eventName.c_str());
		defUseExpr = (defUseExpr < curReadExpr);

		for (unsigned l = 0; l < iw->second.size(); ++l) {
			if (curRead->threadId == iw->second[l]->threadId) //can't read
					continue;
			expr otherWriteExpr = encode.z3_ctx.int_const(
								iw->second[l]->eventName.c_str());
			defUseExpr = defUseExpr && (curReadExpr < otherWriteExpr);
		}
#if PRINT_DEF_USE
		std::cout << "def_use_order_type1" << std::endl;
		std::cout << def_use_order_tpye1 << std::endl;
		std::cout << currentRead->toString() << std::endl;
#endif

	} else{					//read from other thread
		expr curWriteExpr = encode.z3_ctx.int_const(curWrite->eventName.c_str());
		expr curReadExpr = encode.z3_ctx.int_const(curRead->eventName.c_str());
		defUseExpr = (curWriteExpr < curReadExpr);
		for(unsigned i = 0; i < iw->second.size(); ++i){
			if(curWrite->inst->info->id != iw->second[i]->inst->info->id){
				expr otherWriteExpr = encode.z3_ctx.int_const(iw->second[i]->eventName.c_str());
				defUseExpr = defUseExpr
						&& ((otherWriteExpr < curWriteExpr) || (curReadExpr < otherWriteExpr));
			}
		}
#if PRINT_DEF_USE
		std::cout << "def_use_order_type2" << std::endl;
		std::cout << def_use_order_type2 << std::endl;
		std::cout << currentRead->toString() << std::endl;
		std::cout << currentWrite->toString() << std::endl;
#endif

	}

}

Event* DefUseBuilder::getPrevEventInThread(Event* curEvent){
	std::vector<std::vector<Event*>*>::iterator ite = trace->eventList.begin();
	std::vector<std::vector<Event*>*>::iterator iteEnd = trace->eventList.end();
	for( ; ite != iteEnd; ++ite){
		if((*ite) == NULL)
			continue ;
		if((*ite)->front()->threadId == curEvent->threadId){
			unsigned length = (*ite)->size();
			for(unsigned i = 0; i < length; ++i){
				if((*ite)->at(i) == curEvent){
					if(i == 0){
						return NULL;
					} else{
						return (*ite)->at(i - 1);
					}
				}
			}
		}
	}
	return curEvent;
}

Event* DefUseBuilder::getNextEventInThread(Event* curEvent){
	std::vector<std::vector<Event*>*>::iterator ite = trace->eventList.begin();
	std::vector<std::vector<Event*>*>::iterator iteEnd = trace->eventList.end();
	for( ; ite != iteEnd; ++ite){
		if((*ite) == NULL)
			continue ;
		if((*ite)->front()->threadId == curEvent->threadId){
			unsigned length = (*ite)->size();
			for(unsigned i = 0; i < length; ++i){
				if((*ite)->at(i) == curEvent){
					if(i == length - 1){
						return NULL;
					} else{
						return (*ite)->at(i + 1);
					}
				}
			}
		}
	}
	return curEvent;
}

bool DefUseBuilder::isCoveredOrUnsolved(DefUse* defUse, std::vector<DefUse*>& vecSrc){
	std::vector<DefUse*>::iterator duIte = vecSrc.begin();
	std::vector<DefUse*>::iterator duEndIte = vecSrc.end();;
	while(duIte != duEndIte){
		if(defUse->pre == NULL){
			if((*duIte)->pre == NULL)
				if((*duIte)->post->inst->info->assemblyLine ==
					defUse->post->inst->info->assemblyLine  &&
						(*duIte)->post->threadId ==
						defUse->post->threadId )
					return true;
		} else{
			if((*duIte)->pre != NULL)
				if((*duIte)->pre->inst->info->assemblyLine ==
					defUse->pre->inst->info->assemblyLine &&
						 (*duIte)->post->inst->info->assemblyLine ==
						 defUse->post->inst->info->assemblyLine  &&
						 	(*duIte)->pre->threadId ==
						 	defUse->pre->threadId &&
						 		(*duIte)->post->threadId ==
						 		defUse->post->threadId)
					return true;
		}
		++duIte;
	}
	return false;
}

bool DefUseBuilder::isCoveredPrePath(DefUse* defUse){
	return isCoveredOrUnsolved(defUse, rdManager.coveredDefUse_pre);
}

bool DefUseBuilder::isUnsolvedPrePath(DefUse* defUse){
	return isCoveredOrUnsolved(defUse, rdManager.unsolvedDefUse_pre);
}

bool DefUseBuilder::isCoveredExplicit(DefUse* defUse){
	return isCoveredOrUnsolved(defUse, rdManager.explicitDefUse_pre);
}

void DefUseBuilder::removeFromUnsolved(DefUse* defUse){
	std::vector<DefUse*>::iterator duIte = rdManager.unsolvedDefUse_pre.begin();
	std::vector<DefUse*>::iterator duEndIte = rdManager.unsolvedDefUse_pre.end();;
	while(duIte != duEndIte){
		if(defUse->pre == NULL){
			if((*duIte)->pre == NULL)
				if((*duIte)->post->inst->info->assemblyLine ==
					defUse->post->inst->info->assemblyLine  &&
						(*duIte)->post->threadId ==
						defUse->post->threadId ){
					rdManager.unsolvedDefUse_pre.erase(duIte);
					return ;
				}
		} else{
			if((*duIte)->pre != NULL)
				if((*duIte)->pre->inst->info->assemblyLine ==
					defUse->pre->inst->info->assemblyLine &&
						 (*duIte)->post->inst->info->assemblyLine ==
						 defUse->post->inst->info->assemblyLine &&
						 	(*duIte)->pre->threadId ==
						 	defUse->pre->threadId &&
						 		(*duIte)->post->threadId ==
						 		defUse->post->threadId ){
					rdManager.unsolvedDefUse_pre.erase(duIte);
					return ;
				}
		}
		++duIte;
	}
	return ;
}

bool DefUseBuilder::isValid(Event* curRead, Event* curWrite, const expr& defUseExpr){
//	std::cout << defUseExpr << std::endl;

	encode.z3_solver_du.push();
	encode.z3_solver_du.add(defUseExpr);

	addIfFormula(curRead);
	if (curWrite != NULL)
		addIfFormula(curWrite);

	try {
		if(z3::sat == encode.z3_solver_du.check()){
			encode.z3_solver_du.pop();
			return true;
		}
	} catch (z3::exception & ex) {
		std::cout << "\n unexpected error: " << ex << std::endl;
	}

	encode.z3_solver_du.pop();
	return false;
}

void DefUseBuilder::addIfFormula(Event* curEvent) {
	for (unsigned j = 0; j < encode.ifFormula.size(); j++) {
		Event* temp = encode.ifFormula[j].first;
		expr currIf = encode.z3_ctx.int_const(curEvent->eventName.c_str());
		expr tempIf = encode.z3_ctx.int_const(temp->eventName.c_str());
		expr constraint = encode.z3_ctx.bool_val(1);
		if (curEvent->threadId == temp->threadId) {
			if (curEvent->eventId > temp->eventId)
				constraint = encode.ifFormula[j].second;
		} else
			constraint = implies(tempIf < currIf, encode.ifFormula[j].second);
		encode.z3_solver_du.add(constraint);
	}
}

void DefUseBuilder::markLatestWriteForGlobalVar() { 		//called by buildReadWriteFormula
	for (unsigned tid = 0; tid < trace->eventList.size(); tid++) {
		std::vector<Event*>* thread = trace->eventList[tid];
		if (thread == NULL)
			continue;
		for (unsigned index = 0; index < thread->size(); index++) {
			Event* event = thread->at(index);
			if (event->isGlobal) {
				Instruction *I = event->inst->inst;
				if (StoreInst::classof(I)) { //write
					Event* writeEvent;
					map<string, Event*>::iterator it;
					it = latestWriteOneThread.find(event->varName);
					if (it != latestWriteOneThread.end()) {
						writeEvent = it->second;
					} else {
						writeEvent = NULL;
					}
					event->latestWrite = writeEvent;

					latestWriteOneThread[event->varName] = event;
				} else if (!event->implicitGlobalVar.empty()
						&& CallInst::classof(I)) {
					for (unsigned i = 0; i < event->implicitGlobalVar.size(); i++) {

						Event* writeEvent;
						map<string, Event*>::iterator it;
						it = latestWriteOneThread.find(event->varName);
						if (it != latestWriteOneThread.end()) {
							writeEvent = it->second;
						} else {
							writeEvent = NULL;
						}
						event->latestWrite = writeEvent;

						string curr = event->implicitGlobalVar[i];
						string varName = curr.substr(0, curr.find('S', 0));
						latestWriteOneThread[varName] = event;
//						std::cout << "CallInst:" << std::endl;
//						I->dump();
					}
				} else { //read
					Event* writeEvent;
					map<string, Event*>::iterator it;
					it = latestWriteOneThread.find(event->varName);
					if (it != latestWriteOneThread.end()) {
						writeEvent = it->second;
					} else {
						writeEvent = NULL;
					}
					event->latestWrite = writeEvent;
				}
			}
		}
		//post operations
		latestWriteOneThread.clear();
	}
}

void DefUseBuilder::markLatestReadOrWriteForGlobalVar() { //called by buildReadWriteFormula
	std::cout << "create latest read and write!" << std::endl;
	std::map<string, std::vector<Event*> >::iterator iread = trace->allReadSet.begin();
	std::map<string, std::vector<Event*> >::iterator iwrite;

	while(iread != trace->allReadSet.end()) {
		iwrite = trace->allWriteSet.find(iread->first);
		if(iwrite == trace->allWriteSet.end()){
			++iread;
			continue;
		}

		std::vector<Event*>::iterator irEvent = (*iread).second.begin();
		std::vector<Event*>::iterator iwEvent = (*iwrite).second.begin();
		(*irEvent)->latestRead = NULL;
		Event* readEvent = *irEvent;
		Event* writeEvent = NULL;
		while(1){
			//mark latest write for read event;
			while(iwEvent != (*iwrite).second.end()){
				Event* tmp = writeEvent;
				if((*iwEvent)->eventId < (*irEvent)->eventId){
					writeEvent = *iwEvent;
					++iwEvent;
				} else {
					writeEvent = tmp;
					break;
				}
			}
			if(writeEvent){
				if((*irEvent)->threadId == writeEvent->threadId){
					(*irEvent)->latestWrite = writeEvent;
				} else {
					(*irEvent)->latestWrite = NULL;
				}
			} else {
				(*irEvent)->latestWrite = writeEvent; //NULL
			}

			//mark latest read for read event
			if(++irEvent != (*iread).second.end()){
				if((*irEvent)->threadId == readEvent->threadId){
					(*irEvent)->latestRead = readEvent;
				} else {
					(*irEvent)->latestRead = NULL;
				}
				readEvent = *irEvent;
			} else {
				break;
			}

		}


		iwEvent = (*iwrite).second.begin();
		irEvent = (*iread).second.begin();
		(*iwEvent)->latestWrite = NULL;
		writeEvent = *iwEvent;
		readEvent = NULL;
		while(1){
			//mark latest read for write event
			while(irEvent != (*iread).second.end()){
				Event* tmp = readEvent;
				if((*irEvent)->eventId < (*iwEvent)->eventId){
					readEvent = *irEvent;
					++irEvent;
				} else {
					readEvent = tmp;
					break;
				}
			}
			if(readEvent){
				if((*iwEvent)->threadId == readEvent->threadId){
					(*iwEvent)->latestRead = readEvent;
				} else {
					(*iwEvent)->latestRead = NULL;
				}
			} else {
				(*iwEvent)->latestRead = readEvent; //NULL
			}

			//mark latest write for write event
			if(++iwEvent != (*iwrite).second.end()){
				if((*iwEvent)->threadId == writeEvent->threadId){
					(*iwEvent)->latestWrite = writeEvent;
				} else {
					(*iwEvent)->latestWrite = NULL;
				}
				writeEvent =*iwEvent;
			} else {
				break;
			}
		}
		++iread;
	}
	//post operations
}

void DefUseBuilder::sortGlobalSet(std::map<std::string, std::vector<Event *> >& sourceSet) {
	std::map<std::string, std::vector<Event *> >::iterator isourceSet = sourceSet.begin();
	while(isourceSet != sourceSet.end()) {
		stable_sort(isourceSet->second.begin(), isourceSet->second.end(), less_tid);
		++isourceSet;
	}
}

/*
 * reduce readSet and writeSet
 */
void DefUseBuilder::reduceSet(std::map<std::string, std::vector<Event *> >& sourceSet) {
	std::map<std::string, std::vector<Event *> >::iterator it = sourceSet.begin();
	while(it != sourceSet.end()) {
		std::vector<Event *>::iterator eventIt = it->second.begin();
		while(eventIt != it->second.end()) {
			std::vector<Event *>::iterator tmpIt = eventIt + 1;
			while(tmpIt != it->second.end()) {
				if(((*eventIt)->inst->info->assemblyLine == (*tmpIt)->inst->info->assemblyLine) &&
					((*eventIt)->threadId == (*tmpIt)->threadId)) {
					tmpIt = it->second.erase(tmpIt);
				} else {
					tmpIt++;
				}
			}
			eventIt++;
		}
		it++;
	}
}

void DefUseBuilder::selectCRSet(std::vector<expr>& sourceSet){
	std::cout << "\n#################Create New Prefix, Cover multiple CR#################" << std::endl;
	std::cout << "cr set size1: " << sourceSet.size() << std::endl;

	std::vector<expr>::iterator it = sourceSet.begin();
	unsigned cnt = 0;
	while (it != sourceSet.end()) {
	//	std::cout << "the Def-Use constraint:" << std::endl;
	//	std::cout << *it << "\n" << std::endl;
		encode.z3_solver_du.push();
		encode.z3_solver_du.add(*it);

		check_result result;
		try {
			result = encode.z3_solver_du.check();
		} catch (z3::exception& ex) {
			std::cout << "\n unexpected error: " << ex << std::endl;
			continue;
		}

		if (result == z3::sat) {
			it++;
	//		std::cout << "mark\n" << std::endl;
		}else{
			it = sourceSet.erase(it);
			++cnt;
			encode.z3_solver_du.pop();
		}
	}
	std::cout << "delete " << cnt << "CR, all done" << std::endl;
	return ;
}

Event* DefUseBuilder::getFirstPthreadCreateEvent(){
	Event* firPthCreEvent;
	std::map<Event*, uint64_t>::iterator im = trace->createThreadPoint.begin();
	firPthCreEvent = im->first;
	while(++im != trace->createThreadPoint.end()){
		if(im->first->eventId < firPthCreEvent->eventId)
			firPthCreEvent = im->first;
//		std::cout << "create event:" << im->first->toString() << std::endl;
	}
//	std::cout << "create event:" << eFPCreate->toString() << std::endl;
	return firPthCreEvent;
}

void DefUseBuilder::printSingleThreadEvent(){
	std::map<std::string, std::vector<Event*> >::iterator imb, ime;
	for(imb = trace->allReadSet.begin(),
			ime = trace->allReadSet.end(); imb != ime; ++imb){
		std::map<std::string, std::vector<Event*> >::iterator iw;
		iw = trace->allWriteSet.find((*imb).first);
		if(iw == trace->allWriteSet.end())
			continue;

//		std::cout << "print single thread event!" << (*imb).first << std::endl;

		unsigned threadIdTmp = (*(*imb).second[0]).threadId;
		std::vector<Event*>::iterator ivb_r, ive_r;
		for(ivb_r = (*imb).second.begin(),
				ive_r = (*imb).second.end(); ivb_r != ive_r; ++ivb_r){
//			std::cout <<"Read set: " << (*ivb_r)->threadId << " -> " << (*ivb_r)->eventName << std::endl;
			if(threadIdTmp != (*ivb_r)->threadId)
				break;
		}

		if(ivb_r != ive_r)
			continue;

		std::vector<Event*>::iterator ivb_w, ive_w;
		for(ivb_w = (*iw).second.begin(),
				ive_w = (*iw).second.end(); ivb_w != ive_w; ++ivb_w){
//			std::cout <<"Write set: " << (*ivb_w)->threadId << " -> " << (*ivb_w)->eventName << std::endl;
			if(threadIdTmp != (*ivb_w)->threadId)
				break;
		}

		if(ivb_w != ive_w)
			continue;

//		std::cout << "marked!" << (*imb).first << std::endl;
		singleThreadEvent.push_back((*imb).first);
	}

	return ;
}

bool less_tid(const Event* lEvent, const Event* rEvent) {
	return lEvent->threadId < rEvent->threadId;
}

#if PRINT_OPERATION_SET_DETAILED
template <class T> struct displayVector {
		void operator()(T e) const{
			std::cout << e->toString() << endl;
		}
	};

void printOpSet(std::map<std::string, std::vector<Event*> >& opSet){
	std::map<std::string, std::vector<Event*> >::iterator im = opSet.begin();
	for(; im != opSet.end(); ++im){
		std::cout << "value name: " << im->first << "; size: "<< im->second.size() << std::endl;
		for_each(im->second.begin(), im->second.end(), displayVector<Event*>());
		std::cout << endl;
	}
}
#endif

#if PRINT_OPERATION_SET_BRIEFLY
void printOpSetBriefly(std::map<std::string, std::vector<Event*> >& opSet){
	std::map<std::string, std::vector<Event*> >::iterator im = opSet.begin();
	for(; im != opSet.end(); ++im){
		std::cout << "value name: " << im->first << "; size: "<< im->second.size() << std::endl;
		std::vector<Event*>::iterator iv = im->second.begin();
		for(; iv != im->second.end(); ++iv){
			std::cout << (*iv)->eventName << " ->-> " <<
					"line:" << (*iv)->inst->info->line <<
					" ->-> thread id: " << (*iv)->threadId << std::endl;
		}
		std::cout << endl;
	}
}
#endif

} /* namespace klee */


