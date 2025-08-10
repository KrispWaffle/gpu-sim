#include "instruction.hpp"
#include "vartable.hpp"
#include "gpu.hpp"
#include <iostream>
#include <cctype>
#include <stdexcept>

int getRegisterName(std::string _register)
{
    if (_register.length() > 1 && std::isalpha(static_cast<unsigned char>(_register[0])))
    {
        std::string num = _register.substr(1);
        if(num == "TIDX"){
            return -1; // TIDX_RETURN_VAL
        }
        try
        {
            return std::stoi(num);
        }
        catch (...)
        {
            return -2;
        }
    }
    return -2;
}

int getMemoryLocation(std::string mem){
    std::string num = mem.substr(2);
    if(num == "TIDX"){
        return -1; // TIDX_RETURN_VAL
    }
    try
    {
        return std::stoi(num);
    }
    catch(...)
    {
        std::cerr << "ERROR with getting mem location\n";
        return -2;
    }
}
OpInfo decodeOperand(const Operand &op, Thread &t) {
    if (auto pf = std::get_if<float>(&op)) {
        return { OpKind::Constant, *pf,      0,    {} };
    }

    if (auto ps = std::get_if<std::string>(&op)) {
        int tid=t.id();
        const std::string &s = *ps;
        // register?
        if (s.size()>1 && s[0]=='r') {
            int r = getRegisterName(s);
            if(r==0){
                return {OpKind::Register, 0.0f, r, {}};
            }else if(r==-1){
                return {OpKind::Register, 0.0f, tid, {}};
            }
        }else if(s.size()>1 && s.substr(0,2) == "gm" ){
            int g = getMemoryLocation(s) ;
            if(g==0){
                return {OpKind::Global, 0.0f, g, {}};
            }else if(g==-1){
                return {OpKind::Global, 0.0f, tid, {}};
            }
        }else if(s.size()>1 && s.substr(0,2) == "sm" ){
           const int f = getMemoryLocation(s);
            if(f==0){
                return {OpKind::Shared, 0.0f, f, {}};
            }else if(f==-1){
                return {OpKind::Shared, 0.0f, tid, {}};
            }
        }
        // otherwise, variable lookup
        if (auto ov = VarTable::getInstance().getVar(s, t.id())) {
            Variable v = *ov;
            int addr = v.offset;
            float val = v.value;
            return { OpKind::Variable, val, addr, std::move(v) };
        }
    }

    return { OpKind::Invalid, 0.0f, -1, {} };
}
