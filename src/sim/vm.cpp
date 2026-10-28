/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
#include "vm.h"
#include "../core/io.h"
#include <cstdio>
#include <cstring>

namespace st {

static const char* kNames[256] = {0};
struct NameInit { NameInit(){
    kNames[0]="Sleep"; kNames[1]="GenericCall"; kNames[2]="Expression"; kNames[4]="Grab";
    kNames[5]="Drop"; kNames[6]="ChangeSuit"; kNames[7]="Refresh"; kNames[8]="RandomNumber";
    kNames[11]="GetDistanceTo"; kNames[12]="GetDirectionTo"; kNames[13]="PushInteraction";
    kNames[14]="FindBestObjectForFunction"; kNames[15]="BreakPoint"; kNames[16]="FindLocationFor";
    kNames[17]="IdleForInput"; kNames[18]="RemoveObjectInstance"; kNames[20]="RunFunctionalTree";
    kNames[22]="LookTowards"; kNames[23]="PlaySound"; kNames[24]="Relationship"; kNames[25]="TransferFunds";
    kNames[26]="Relationship2"; kNames[27]="GotoRelativePosition"; kNames[28]="RunTreeByName";
    kNames[29]="SetMotiveChange"; kNames[30]="SysLog"; kNames[31]="SetToNext"; kNames[32]="TestObjectType";
    kNames[35]="SpecialEffect"; kNames[36]="DialogPrivate"; kNames[37]="TestSimInteractingWith";
    kNames[38]="DialogGlobal"; kNames[39]="DialogSemiGlobal"; kNames[40]="OnlineJobsCall";
    kNames[41]="SetBalloonHeadline"; kNames[42]="CreateObjectInstance"; kNames[43]="DropOnto";
    kNames[44]="AnimateSim"; kNames[45]="GotoRoutingSlot"; kNames[46]="Snap"; kNames[47]="Reach";
    kNames[48]="StopAllSounds"; kNames[49]="NotifyOutOfIdle"; kNames[50]="ChangeActionString";
    kNames[62]="InvokePlugin"; kNames[63]="GetTerrainInfo"; kNames[65]="FindBestAction";
    kNames[67]="InventoryOperations";
}} _nameInit;

const char* opcodeName(uint16_t op) {
    if (op < 256) return kNames[op] ? kNames[op] : "Primitive?";
    return "SubRoutine";
}

void ExprOperand::decode(const uint8_t* o) {
    Reader r(o, 8, Endian::Little);
    lhsData = r.s16(); rhsData = r.s16(); isSigned = r.u8();
    op = (ExprOp)r.u8(); lhsOwner = (Scope)r.u8(); rhsOwner = (Scope)r.u8();
}

static const char* scopeName(Scope s) {
    switch(s){
        case Scope::MyObjectAttributes: return "MyAttr";
        case Scope::StackObjectAttributes: return "StkAttr";
        case Scope::MyObject: return "MyObj";
        case Scope::StackObject: return "StkObj";
        case Scope::Global: return "Global";
        case Scope::Literal: return "Lit";
        case Scope::Temps: return "Temp";
        case Scope::Parameters: return "Param";
        case Scope::StackObjectID: return "StkObjID";
        case Scope::Local: return "Local";
        case Scope::MyMotives: return "MyMotive"; case Scope::StackObjectMotives: return "StkMotive";
        case Scope::MyPersonData: return "MyPerson"; case Scope::StackObjectPersonData: return "StkPerson";
    }
    return "Scope?";
}
static const char* opName(ExprOp o){
    switch(o){case ExprOp::GreaterThan:return ">";case ExprOp::LessThan:return "<";
        case ExprOp::Equals:return "==";case ExprOp::PlusEquals:return "+=";case ExprOp::MinusEquals:return "-=";
        case ExprOp::Assign:return "=";case ExprOp::MulEquals:return "*=";case ExprOp::DivEquals:return "/=";
        case ExprOp::IsFlagSet:return "flagSet";case ExprOp::SetFlag:return "setFlag";case ExprOp::ClearFlag:return "clrFlag";
        case ExprOp::IncAndLessThan:return "++<";case ExprOp::ModEquals:return "%=";case ExprOp::AndEquals:return "&=";
        case ExprOp::GreaterThanOrEqualTo:return ">=";case ExprOp::LessThanOrEqualTo:return "<=";
        case ExprOp::NotEqualTo:return "!=";case ExprOp::DecAndGreaterThan:return "-->";
        case ExprOp::Push:return "push";case ExprOp::Pop:return "pop";}
    return "?";
}
static void fmtOperand(char* buf,size_t n,Scope s,int16_t d){
    if(s==Scope::Literal) snprintf(buf,n,"%d",d);
    else snprintf(buf,n,"%s[%d]",scopeName(s),d);
}
std::string disassembleExpr(const uint8_t* operand){
    ExprOperand e; e.decode(operand);
    char l[32],r[32]; fmtOperand(l,sizeof l,e.lhsOwner,e.lhsData); fmtOperand(r,sizeof r,e.rhsOwner,e.rhsData);
    char out[96]; snprintf(out,sizeof out,"%s %s %s",l,opName(e.op),r);
    return out;
}

const Bhav* VM::resolve(uint16_t treeId) {
    static Bhav cache; // single-slot cache for simplicity
    const Chunk* c = obj ? obj->get("BHAV", treeId) : nullptr;
    if (!c && global) c = global->get("BHAV", treeId);
    if (!c) return nullptr;
    cache = Bhav();
    if (!cache.parse(*c)) return nullptr;
    return &cache;
}

int32_t VM::getVar(VMFrame& f, Scope s, int16_t data) {
    switch (s) {
        case Scope::Literal: return data;
        case Scope::Temps: return (data>=0 && data<32) ? temps[data] : 0;
        case Scope::Local: return ((size_t)data < f.locals.size()) ? f.locals[data] : 0;
        case Scope::Parameters: return ((size_t)data < f.params.size()) ? f.params[data] : 0;
        case Scope::MyObjectAttributes:
        case Scope::StackObjectAttributes: { auto it=attrs.find(data); return it==attrs.end()?0:it->second; }
        case Scope::Global: { auto it=globals.find(data); return it==globals.end()?0:it->second; }
        case Scope::MyMotives: case Scope::StackObjectMotives: return (data>=0&&data<16)? simMotives[data] : 0;
        case Scope::MyPersonData: case Scope::StackObjectPersonData: return (data>=0&&data<256)? personData[data] : 0;
        default: return 0;
    }
}
bool VM::setVar(VMFrame& f, Scope s, int16_t data, int32_t val) {
    switch (s) {
        case Scope::Temps: if(data>=0&&data<32){temps[data]=val;return true;} return false;
        case Scope::Local: if(data>=0){ if((size_t)data>=f.locals.size()) f.locals.resize(data+1,0); f.locals[data]=val; return true;} return false;
        case Scope::Parameters: if(data>=0){ if((size_t)data>=f.params.size()) f.params.resize(data+1,0); f.params[data]=val; return true;} return false;
        case Scope::MyObjectAttributes:
        case Scope::StackObjectAttributes: attrs[data]=val; return true;
        case Scope::Global: globals[data]=val; return true;
        case Scope::MyMotives: case Scope::StackObjectMotives: if(data>=0&&data<16){simMotives[data]=val;return true;} return false;
        case Scope::MyPersonData: case Scope::StackObjectPersonData: if(data>=0&&data<256){personData[data]=val;return true;} return false;
        case Scope::Literal: return false; // can't assign to a literal
        default: return false;
    }
}

int VM::execExpression(VMFrame& f, const uint8_t* operand) {
    ExprOperand e; e.decode(operand);
    int32_t lhs, rhs; bool res=false;
    auto L=[&]{ return getVar(f,e.lhsOwner,e.lhsData); };
    auto R=[&]{ return getVar(f,e.rhsOwner,e.rhsData); };
    switch (e.op) {
        case ExprOp::Assign:  rhs=R(); return setVar(f,e.lhsOwner,e.lhsData,rhs)?0:1;
        case ExprOp::GreaterThan: return (L()>R())?0:1;
        case ExprOp::LessThan:    return (L()<R())?0:1;
        case ExprOp::Equals:      return (L()==R())?0:1;
        case ExprOp::NotEqualTo:  return (L()!=R())?0:1;
        case ExprOp::GreaterThanOrEqualTo: return (L()>=R())?0:1;
        case ExprOp::LessThanOrEqualTo:    return (L()<=R())?0:1;
        case ExprOp::PlusEquals:  lhs=L(); rhs=R(); return setVar(f,e.lhsOwner,e.lhsData,lhs+rhs)?0:1;
        case ExprOp::MinusEquals: lhs=L(); rhs=R(); return setVar(f,e.lhsOwner,e.lhsData,lhs-rhs)?0:1;
        case ExprOp::MulEquals:   lhs=L(); rhs=R(); return setVar(f,e.lhsOwner,e.lhsData,lhs*rhs)?0:1;
        case ExprOp::DivEquals:   lhs=L(); rhs=R(); if(rhs==0)return 1; return setVar(f,e.lhsOwner,e.lhsData,lhs/rhs)?0:1;
        case ExprOp::ModEquals:   lhs=L(); rhs=R(); if(rhs==0)return 1; return setVar(f,e.lhsOwner,e.lhsData,((lhs%rhs)+rhs)%rhs)?0:1;
        case ExprOp::AndEquals:   lhs=L(); rhs=R(); return setVar(f,e.lhsOwner,e.lhsData,lhs&rhs)?0:1;
        case ExprOp::IncAndLessThan: lhs=L()+1; setVar(f,e.lhsOwner,e.lhsData,lhs); return (lhs<R())?0:1;
        case ExprOp::DecAndGreaterThan: lhs=L()-1; setVar(f,e.lhsOwner,e.lhsData,lhs); return (lhs>R())?0:1;
        case ExprOp::SetFlag:   lhs=L(); rhs=R(); return setVar(f,e.lhsOwner,e.lhsData, lhs|(1<<(rhs-1)))?0:1;
        case ExprOp::ClearFlag: lhs=L(); rhs=R(); return setVar(f,e.lhsOwner,e.lhsData, lhs&~(1<<(rhs-1)))?0:1;
        case ExprOp::IsFlagSet: lhs=L(); rhs=R(); return ((lhs&(1<<(rhs-1)))!=0)?0:1;
        default: (void)res; return 1;
    }
}

VM::Result VM::run(const Bhav& entry, std::vector<int32_t> params, int depth) {
    if (depth > 64) return R_ERROR;
    VMFrame f; f.bhav=&entry; f.ip=0; f.params=std::move(params);
    f.locals.assign(entry.locals ? entry.locals : 4, 0);

    int steps=0;
    while (f.ip >= 0 && (size_t)f.ip < entry.insns.size()) {
        if (++steps > 100000) return R_ERROR;
        const BhavInstruction& ins = entry.insns[f.ip];
        int rc; // 0 true, 1 false, 2 error
        if (ins.opcode == 2) {
            rc = execExpression(f, ins.operand);
            if (trace) { char b[160]; snprintf(b,sizeof b,"  @%d Expression: %s  -> %s", f.ip, disassembleExpr(ins.operand).c_str(), rc==0?"T":"F"); log.push_back(b); }
        } else if (ins.opcode == 30) { // SysLog
            char b[96]; snprintf(b,sizeof b,"  @%d SysLog temp0=%d temp1=%d", f.ip, temps[0], temps[1]); log.push_back(b); rc=0;
        } else if (ins.opcode == 0) { // Sleep -> treat as pass
            if (trace){char b[48];snprintf(b,sizeof b,"  @%d Sleep",f.ip);log.push_back(b);} rc=0;
        } else if (ins.opcode == 8) { // RandomNumber (deterministic stub)
            rc=0;
        } else if (ins.opcode == 31) { // SetToNext: iterate lot objects — no object list yet => end of iteration
            if (trace){char b[64];snprintf(b,sizeof b,"  @%d SetToNext -> no more objects (F)",f.ip);log.push_back(b);} rc=1;
        } else if (ins.opcode >= 256) { // sub-routine call (private/global tree)
            const Bhav* sub = resolve(ins.opcode);
            if (trace){char b[64];snprintf(b,sizeof b,"  @%d call tree %u %s",f.ip,ins.opcode,sub?"":"(unresolved)");log.push_back(b);}
            if (!sub) rc=1;
            else { Result sr = run(*sub, {}, depth+1); rc = (sr==R_TRUE)?0:(sr==R_FALSE)?1:2; }
        } else {
            if (trace){char b[80];snprintf(b,sizeof b,"  @%d %s (opcode %u) [stub->true]",f.ip,opcodeName(ins.opcode),ins.opcode);log.push_back(b);}
            rc=0; // stub unimplemented primitive as true so the trace continues
        }

        uint8_t ptr = (rc==0) ? ins.truePtr : ins.falsePtr;
        if (rc==2) return R_ERROR;
        if (ptr==255) return R_FALSE;
        if (ptr==254) return R_TRUE;
        if (ptr==253) return R_ERROR;
        f.ip = ptr;
    }
    return R_TRUE;
}

} // namespace st
