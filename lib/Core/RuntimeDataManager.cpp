/*
 * RuntimeDataManager.cpp
 *
 *  Created on: Jun 10, 2014
 *      Author: ylc
 */

#include "RuntimeDataManager.h"

#include "Transfer.h"
#if LLVM_VERSION_CODE >= LLVM_VERSION(3, 3)
#include "llvm/IR/Constants.h"
#include "llvm/Support/raw_ostream.h"
#else
#include "llvm/Constants.h"
#endif
#include <iostream>

using namespace std;
using namespace llvm;

namespace klee {

RuntimeDataManager::RuntimeDataManager() :
		currentTrace(NULL), pArgv(NULL), iArgc(0) {
	// TODO Auto-generated constructor stub
	traceList.reserve(20);
	allFormulaNum = 0;
	allGlobal = 0;
	brGlobal = 0;
	solvingTimes = 0;
	satBranch = 0;
	unSatBranch = 0;
	uunSatBranch = 0;
	solvingCost = 0.0;
	runningCost = 0.0;
	inputCost = 0.0;
	satCost = 0.0;
	unSatCost = 0.0;
	runState = 0;
	allMP = 0;
}

RuntimeDataManager::~RuntimeDataManager() {
	// TODO Auto-generated destructor stub


	string ErrorInfo;
	raw_fd_ostream out_to_file("./output_info/statics.txt", ErrorInfo, 0x0202);
	stringstream ss;
	ss << "AllFormulaNum:" << allFormulaNum << "\n";
	ss << "SovingTimes:" << solvingTimes << "\n";
	ss << "TotalNewPath:" << testedTraceList.size() << "\n";
	ss << "TotalOldPath:" << traceList.size() - testedTraceList.size() << "\n";
	ss << "TotalPath:" << traceList.size() << "\n";
	if (testedTraceList.size()) {
		ss << "allGlobal:" << allGlobal * 1.0 / testedTraceList.size() << "\n";
		ss << "brGlobal:" << brGlobal * 1.0 / testedTraceList.size() << "\n";
	} else {
		ss << "allGlobal:0" << "\n";
		ss << "brGlobal:0" << "\n";
	}
	if (testedTraceList.size()) {
		ss << "AllBranch:" << ( satBranch + unSatBranch ) * 1.0 / testedTraceList.size() << "\n";
		ss << "satBranch:" << satBranch * 1.0 / testedTraceList.size() << "\n";
	} else {
		ss << "AllBranch:0" << "\n";
		ss << "satBranch:0" << "\n";
	}
	if (satBranch) {
		ss << "satCost:" << satCost / satBranch << "\n";
	} else {
		ss << "satCost:0" << "\n";
	}
	if (testedTraceList.size()) {
		ss << "unSatBranch:" << unSatBranch * 1.0 / testedTraceList.size()  << "\n";
	} else {
		ss << "unSatBranch:0" << "\n";
	}
	if (unSatBranch) {
		ss << "unSatCost:" << unSatCost / unSatBranch << "\n";
	} else {
		ss << "unSatCost:0" << "\n";
	}
	if (testedTraceList.size()) {
		ss << "uunSatBranch:" << uunSatBranch * 1.0 / testedTraceList.size()  << "\n";
	} else {
		ss << "uunSatBranch:0" << "\n";
	}
	ss << "SolvingCost:" << solvingCost << "\n";
	ss << "RunningCost:" << runningCost << "\n";
	ss << "inputCost:" << inputCost << "\n";
	ss << "already negated branch:" << alreadyNegatedBB.size() << "\n";
	ss << "all MP:" << allMP << "\n";
	ss << "left MP:" << MP.size() << "\n";
	ss << "covered  def-use size:" << coveredDefUse_pre.size() << "\n";
	ss << "explicit def-use size:" << this->explicitDefUse_pre.size() << "\n";
	ss << "implicit def-use size:" << this->implicitDefUse_pre.size() << "\n";
	ss << "unsolved def-use size:" << this->unsolvedDefUse_pre.size() << "\n";
	out_to_file << ss.str();
	out_to_file.close();

	raw_fd_ostream duout_to_file("./output_info/du.txt", ErrorInfo, 0x0202);
	stringstream duss;
	for (unsigned i = 0; i < explicitDefUse_pre.size(); i++) {
		if (explicitDefUse_pre[i]->pre == NULL) {
			duss << "NULL ";
		} else {
			duss << explicitDefUse_pre[i]->pre->inst->info->line << " ";
		}
			duss << explicitDefUse_pre[i]->post->inst->info->line << "\n";
	}
//	duss << "unsolved du: " << "\n";
//
//	for (unsigned i = 0; i < unsolvedDefUse_pre.size(); i++) {
//		if (unsolvedDefUse_pre[i]->pre != NULL) {
//			duss << unsolvedDefUse_pre[i]->pre->toString() << " ";
//		}
//		duss << unsolvedDefUse_pre[i]->post->toString() << "\n";
//	}
	duout_to_file << duss.str();
	duout_to_file.close();

	for (vector<Trace*>::iterator ti = traceList.begin(), te = traceList.end();
			ti != te; ti++) {
		delete *ti;
	}

	/* added : Apr 5, 2016
	 * Author: hhfan
	 */
	//free memory space
	std::vector<DefUse*>::iterator coveredDuIte = coveredDefUse_pre.begin();
	while(coveredDuIte != coveredDefUse_pre.end()){
		delete *coveredDuIte;
		*coveredDuIte = NULL;
		coveredDuIte++;
	}
	std::vector<DefUse*>::iterator unsolvedDuIte = unsolvedDefUse_pre.begin();
	while(unsolvedDuIte != unsolvedDefUse_pre.end()){
		delete *unsolvedDuIte;
		*unsolvedDuIte = NULL;
		unsolvedDuIte++;
	}

	coveredDefUse_pre.clear();
	unsolvedDefUse_pre.clear();
	explicitDefUse_pre.clear();
	implicitDefUse_pre.clear();

	std::vector<DefUse*>().swap(coveredDefUse_pre);
	std::vector<DefUse*>().swap(unsolvedDefUse_pre);
	std::vector<DefUse*>().swap(explicitDefUse_pre);
	std::vector<DefUse*>().swap(implicitDefUse_pre);
}

Trace* RuntimeDataManager::createNewTrace(unsigned traceId) {
	currentTrace = new Trace();
	currentTrace->Id = traceId;
	traceList.push_back(currentTrace);
	return currentTrace;
}

Trace* RuntimeDataManager::getCurrentTrace() {
	return currentTrace;
}

void RuntimeDataManager::addScheduleSet(Prefix* prefix) {
	scheduleSet.push_back(prefix);
}

void RuntimeDataManager::printCurrentTrace(bool file) {
	currentTrace->print(file);
}

Prefix* RuntimeDataManager::getNextPrefix() {
	if (scheduleSet.empty()) {
		return NULL;
	} else {
		Prefix* prefix = scheduleSet.front();
		scheduleSet.pop_front();
		return prefix;
	}
}

void RuntimeDataManager::clearAllPrefix() {
	scheduleSet.clear();
}

bool RuntimeDataManager::isCurrentTraceUntested() {
	bool result = true;
	for (set<Trace*>::iterator ti = testedTraceList.begin(), te =
			testedTraceList.end(); ti != te; ti++) {
		if (currentTrace->isEqual(*ti)) {
			result = false;
			break;
		}
	}
	currentTrace->isUntested = result;
	if (result) {
		testedTraceList.insert(currentTrace);
	}
	return result;
}

void RuntimeDataManager::printCurrPrefix(Prefix *prefix, ostream &out) {
	prefix->print(out);
}

void RuntimeDataManager::printAllPrefix(ostream &out) {
	out << "num of prefix: " << scheduleSet.size() << endl;
	unsigned num = 1;
	for (list<Prefix*>::iterator pi = scheduleSet.begin(), pe =
			scheduleSet.end(); pi != pe; pi++) {
		out << "Prefix " << num << endl;
		(*pi)->print(out);
		num++;
	}
}

void RuntimeDataManager::printAllTrace(ostream &out) {
	out << "\nTrace Info:\n";
	out << "num of trace: " << traceList.size() << endl << endl;
	unsigned num = 1;
	for (vector<Trace*>::iterator ti = traceList.begin(), te = traceList.end();
			ti != te; ti++) {
		Trace* trace = *ti;
		if (trace->isUntested) {
			out << "Trace " << num << endl;
			if (trace->abstract.empty()) {
				trace->createAbstract();
			}
			for (vector<string>::iterator ai = trace->abstract.begin(), ae =
					trace->abstract.end(); ai != ae; ai++) {
				out << *ai << endl;
			}
			out << endl;
			num++;
		}
	}
}

int RuntimeDataManager::getPrefixSetSize() {
	return scheduleSet.size();
}

}


