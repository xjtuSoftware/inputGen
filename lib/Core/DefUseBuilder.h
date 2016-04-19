/*
 * DefUseBuilder.h
 *
 *  Created on: Apr 5, 2016
 *      Author: hhfan
 */

#ifndef LIB_CORE_DEFUSEBUILDER_H_
#define LIB_CORE_DEFUSEBUILDER_H_



#include "Event.h"
#include "RuntimeDataManager.h"
#include "Trace.h"
#include "Encode.h"

#include <z3++.h>
#include <stack>
#include <vector>
#include <utility>
#include <algorithm>
#include <functional>

using namespace llvm;
using namespace z3;
using namespace std;
namespace klee {

class DefUseBuilder {
private:
	RuntimeDataManager& rdManager;
	Encode& encode;
	Trace* trace; //all data about encoding
	map<string, Event*> latestWriteOneThread;
	map<string, Event*> latestReadOneThread;
	vector<string> singleThreadEvent;

	Event* currentEvent;

public:
	DefUseBuilder(RuntimeDataManager& data_t, Encode& encode_t);
	virtual ~DefUseBuilder();

private:
	void buildDefUseForCurPath();
	void buildAndVerifyDefUse(Event*, Event*, map<string, vector<Event *> >::iterator);
	void buildDefUse(Event*, Event*, map<string, vector<Event *> >::iterator);
	Event* getLatestWriteInCurPath(Event*, map<string, vector<Event *> >::iterator);

	void buildExpr(Event*, Event*, expr&);
	void buildExpr_AddAllWrite(Event*, Event*, map<string, vector<Event *> >::iterator, expr&);
	Event* getPrevEventInThread(Event*);
	Event* getNextEventInThread(Event*);
	bool isCoveredOrUnsolved(DefUse*, std::vector<DefUse*>&);
	bool isCoveredPrePath(DefUse*);
	bool isUnsolvedPrePath(DefUse*);
	bool isCoveredExplicit(DefUse*);
	void removeFromUnsolved(DefUse*);
	bool isValid(Event*, Event*, const expr&);
	void addIfFormula(Event*);
	void printDefUse(DefUse* def_use);
	void printDefUse_ss(DefUse*);

	void markLatestWriteForGlobalVar();
	void markLatestReadOrWriteForGlobalVar();
	void sortGlobalSet(std::map<std::string, std::vector<Event *> >& sourceSet);
	void reduceSet(std::map<std::string, std::vector<Event *> >& sourceSet);

	void selectCRSet(std::vector<expr>& sourceSet);

	Event* getFirstPthreadCreateEvent();
	void printSingleThreadEvent();

public:
	void buildAllDefUse();
};


	bool less_tid(const Event * lEvent, const Event*);
	void printOpSet(std::map<std::string, std::vector<Event*> >&);
	void printOpSetBriefly(std::map<std::string, std::vector<Event*> >&);
	template <class T> struct displayVector;
} /* namespace klee */



#endif /* LIB_CORE_DEFUSEBUILDER_H_ */
