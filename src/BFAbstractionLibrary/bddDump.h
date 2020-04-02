//
// C++ Implementation: bddDump
//
// Description: 
//  Tool class for dumping BDDs to a nice .DOT file
//
// Author: Ruediger Ehlers <ehlers@grace>, (C) 2008
//
// Copyright: See COPYING file that comes with this distribution

#ifndef _BDD_DUMP_H
#define _BDD_DUMP_H

#include "BF.h"
#include <sstream>
#include <cstdlib>

/**
 This class represents some structure of which the variable structure of a BDD can be obtained.
 */
class VariableInfoContainer {
public:
	virtual ~VariableInfoContainer() {};
	virtual void getVariableTypes(std::vector<std::string> &types) const = 0;
	virtual void getVariableNumbersOfType(std::string type, std::vector<unsigned int> &nums) const = 0;
	virtual BF getVariableBF(unsigned int number) const = 0;
	virtual std::string getVariableName(unsigned int number) const = 0;
};

// void BDD_oldDumpDot(const VariableInfoContainer &cont, BF b, const char* filename);
void BF_newDumpDot(const VariableInfoContainer &cont, const BF &b, const char* varOrder, const std::string filename);

class BFDumpDotException {
protected:
    std::string message;
public:
    BFDumpDotException(const std::string &msg) : message(msg) {};
	~BFDumpDotException() {};
	BFDumpDotException(const char* file, int line) {
		std::ostringstream s;
		s << "BFDumpDotException thrown: " << file << ", line " << line;
		message = s.str();
	}
	const std::string getMessage() const { return message; };
};

//=============================================================
// The BFDumpDot library can also be used for checking BF types
// at runtime. The following macros makes sure that:
// - BDDs are dumped if the BF_DUMPDOT directive is set
// - BDDs are type-checked if the "assert" command is operational (i.e., NDEBUG is not set) and the
//   BF_DUMPDOT directive is not set
// - nothing is done otherwise.
//=============================================================
inline bool BF_hasVariableSupport(const VariableInfoContainer &cont, const BF &toCheck, const char *varOrder) {
    try {
        BF_newDumpDot(cont, toCheck, varOrder, "/dev/null");
        return true;
    } catch (BFDumpDotException e) {
        return false;
    }
}

#ifndef NDEBUG
#ifdef BF_DUMPDOT
#define BF_DUMPANDCHECK(cont,bf,varOrder,f1) \
    BF_newDumpDot(cont,bf,varOrder,f1);
#define BF_DUMPANDCHECK2(cont,bf,varOrder,f1,f2) \
   { \
       std::ostringstream os; \
       os << f1 << f2; \
       BF_newDumpDot(cont,bf,varOrder,os.str()); \
   }
#define BF_DUMPANDCHECK3(cont,bf,varOrder,f1,f2,f3) \
   { \
       std::ostringstream os; \
       os << f1 << f2 << f3; \
       BF_newDumpDot(cont,bf,varOrder,os.str()); \
   }
#define BF_DUMPANDCHECK4(cont,bf,varOrder,f1,f2,f3,f4) \
   { \
       std::ostringstream os; \
       os << f1 << f2 << f3 << f4; \
       BF_newDumpDot(cont,bf,varOrder,os.str()); \
   }
#define BF_DUMPANDCHECK5(cont,bf,varOrder,f1,f2,f3,f4,f5) \
   { \
       std::ostringstream os; \
       os << f1 << f2 << f3 << f4 << f5; \
       BF_newDumpDot(cont,bf,varOrder,os.str()); \
   }

#else
#define BF_DUMPANDCHECK(cont,bf,varOrder,f1) \
    BF_hasVariableSupport(cont,bf,varOrder);
#define BF_DUMPANDCHECK2(cont,bf,varOrder,f1,f2) \
    BF_hasVariableSupport(cont,bf,varOrder);
#define BF_DUMPANDCHECK3(cont,bf,varOrder,f1,f2,f3) \
    BF_hasVariableSupport(cont,bf,varOrder);
#define BF_DUMPANDCHECK4(cont,bf,varOrder,f1,f2,f3,f4) \
    BF_hasVariableSupport(cont,bf,varOrder);
#define BF_DUMPANDCHECK5(cont,bf,varOrder,f1,f2,f3,f4,f5) \
    BF_hasVariableSupport(cont,bf,varOrder);

#endif
#else // Now NDEBUG!
    #define BF_DUMPANDCHECK(cont,bf,varOrder,f1) {}
    #define BF_DUMPANDCHECK2(cont,bf,varOrder,f1,f2) {}
    #define BF_DUMPANDCHECK3(cont,bf,varOrder,f1,f2,f3) {}
    #define BF_DUMPANDCHECK4(cont,bf,varOrder,f1,f2,f3,f4) {}
    #define BF_DUMPANDCHECK5(cont,bf,varOrder,f1,f2,f3,f4,f5) {}


#endif

// End of include file
#endif
