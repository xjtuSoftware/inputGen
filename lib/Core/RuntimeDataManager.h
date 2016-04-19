/*
 * RuntimeDataManager.h
 *
 *  Created on: Jun 10, 2014
 *      Author: ylc
 */

#ifndef RUNTIMEDATAMANAGER_H_
#define RUNTIMEDATAMANAGER_H_

#include "Trace.h"
#include "Prefix.h"
#include <set>
namespace klee {


typedef struct DefineUseChain {
	Event* pre; //if=null, stand for initial event.
	Event* post;
} DefUse;

class RuntimeDataManager {

private:
	std::vector<Trace*> traceList; // store all traces;
	Trace* currentTrace; // trace associated with current execution
	std::set<Trace*> testedTraceList; // traces which have been examined
	std::list<Prefix*> scheduleSet; // prefixes which have not been examined


	//first: main input, second: prefix executed to negated branch.


	/*added : Apr 5, 2016
	 *Author: hhfan
	 *Detail: Define-Use Chain Set.
	 */
public:
	std::vector<DefUse*> coveredDefUse_pre;		//def-use(s) has been covered in previous paths.
	std::vector<DefUse*> explicitDefUse_pre;
	std::vector<DefUse*> implicitDefUse_pre;
	std::vector<DefUse*> unsolvedDefUse_pre;

public:
	//newly added stastic info
	unsigned allFormulaNum;
	unsigned allGlobal;
	unsigned brGlobal;
	unsigned solvingTimes;
	unsigned satBranch;
	unsigned unSatBranch;
	unsigned uunSatBranch;
	double runningCost;
	double solvingCost;
	double inputCost;
	double satCost;
	double unSatCost;

	unsigned allMP;

	char **pArgv;
	int iArgc;

	unsigned runState;

	std::set<uint64_t> globalVarAddress;
	std::set<llvm::BasicBlock*> bbOpGlobal;

//	std::map<std::vector<std::string>, Prefix*> symbolicInputPrefix;
	std::map<Prefix*, std::vector<std::string> > symbolicInputPrefix;
	std::map<Prefix*, std::map<std::string, unsigned> > intInputPrefix;
	std::map<std::string, unsigned> intArgv;
	// if.then or if.else corresponding BasicBlock
	std::map<std::string, llvm::BasicBlock*> ifBB;
	std::map<llvm::BasicBlock*, std::set<std::string> > bbOpGVarName;
	std::multimap<std::string, std::string> MP;
	std::map<std::string, std::set<std::string> > waitingSet;
	std::set<std::string> possibleBranch;

	std::set<llvm::BasicBlock*> alreadyNegatedBB;

	RuntimeDataManager();
	virtual ~RuntimeDataManager();

	Trace* createNewTrace(unsigned traceId);
	Trace* getCurrentTrace();
	void addScheduleSet(Prefix* prefix);
	void printCurrentTrace(bool file);
	Prefix* getNextPrefix();
	void clearAllPrefix();
	bool isCurrentTraceUntested();
	void printAllPrefix(std::ostream &out);
	void printAllTrace(std::ostream &out);
	int getPrefixSetSize();

	void printCurrPrefix(Prefix *prefix, std::ostream &out);
};

}
#endif /* RUNTIMEDATAMANAGER_H_ */
