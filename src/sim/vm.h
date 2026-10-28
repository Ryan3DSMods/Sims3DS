/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// A minimal SimAntics interpreter: enough of the VM to execute real BHAV
// bytecode (expressions, control flow, private sub-routine calls).
// Reference: FreeSO/FreeSims SimsAntics engine. This is the M3 kickoff core,
// not the full primitive set.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include "../formats/iff.h"

namespace st {

// SimAntics variable scopes (subset we resolve; others read 0 / log).
enum class Scope : uint8_t {
    MyObjectAttributes = 0, StackObjectAttributes = 1, MyObject = 3, StackObject = 4,
    Global = 6, Literal = 7, Temps = 8, Parameters = 9, StackObjectID = 10,
    MyMotives = 14, StackObjectMotives = 15, MyPersonData = 18, StackObjectPersonData = 19, Local = 25,
};

enum class ExprOp : uint8_t {
    GreaterThan=0, LessThan=1, Equals=2, PlusEquals=3, MinusEquals=4, Assign=5,
    MulEquals=6, DivEquals=7, IsFlagSet=8, SetFlag=9, ClearFlag=10, IncAndLessThan=11,
    ModEquals=12, AndEquals=13, GreaterThanOrEqualTo=14, LessThanOrEqualTo=15,
    NotEqualTo=16, DecAndGreaterThan=17, Push=18, Pop=19,
};

struct ExprOperand {
    int16_t lhsData=0, rhsData=0;
    uint8_t isSigned=0;
    ExprOp  op=ExprOp::Assign;
    Scope   lhsOwner=Scope::Temps, rhsOwner=Scope::Literal;
    void decode(const uint8_t* o);
};

const char* opcodeName(uint16_t op);
std::string disassembleExpr(const uint8_t* operand);

// Execution context for one running object.
struct VMFrame {
    const Bhav* bhav = nullptr;
    int ip = 0;
    std::vector<int32_t> locals;
    std::vector<int32_t> params;
};

struct VM {
    IffFile* obj = nullptr;       // object IFF whose BHAVs we call
    IffFile* global = nullptr;    // global.iff (256..) — optional
    int32_t temps[32] = {0};
    std::map<int,int32_t> attrs;  // MyObjectAttributes
    std::map<int,int32_t> globals;
    // the Sim performing the action (caller): motives -100..100, person data
    int32_t simMotives[16] = {0};
    int32_t personData[256] = {0};
    bool trace = false;
    std::vector<std::string> log; // SysLog output + trace lines

    const Bhav* resolve(uint16_t treeId);
    int32_t getVar(VMFrame& f, Scope s, int16_t data);
    bool    setVar(VMFrame& f, Scope s, int16_t data, int32_t val);

    // returns true/false (tree result); depth-limited.
    enum Result { R_TRUE, R_FALSE, R_ERROR };
    Result run(const Bhav& entry, std::vector<int32_t> params = {}, int depth = 0);

private:
    // primitive returns 0 goto-true, 1 goto-false, 2 error
    int execExpression(VMFrame& f, const uint8_t* operand);
};

} // namespace st
