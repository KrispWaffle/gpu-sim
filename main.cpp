#include <iostream>
#include <vector>
#include <algorithm>
#include <variant>
#include <string>
#include <stdexcept>
#define NUM_THREADS 32
#define NUM_REGISTERS 2

enum class Opcode {
    ADD, MUL, LD, ST, MOV,
};

enum class states{
    IDLE,
    ACTIVE,
    FINISHED 
};
using Operand = std::variant<Opcode,std::string, float>;

struct Instr {        
    Opcode op;
    std::vector<Operand> src; 
    std::string src_line;

};

int mem[] = {0, 0, 0, 0, 0};

int getRegisterName(std::string _register ){
    if (_register.length() > 1 && _register[0] == 'r') {
        std::string num = _register.substr(1);
        try
        {
            return std::stoi(num);
        }
        catch(const std::exception& e)
        {
            return -1;
        }
        
    }
    return -1;
}
class Thread{
    
    public:

      

    Thread(): id_(_id++), _registers(NUM_REGISTERS), state(states::IDLE){}
    int id()  { return _id; }
    void set_instruction(Instr _instruction ) { intruction = _instruction;}
    void run(){
        state = states::ACTIVE;
        std::vector<int> t_reg_num; 
        float t_value;
        Opcode t_operation; 
        for (auto i: intruction.src){
            
            if(auto value = std::get_if<Opcode>(&i)){
                Opcode& v = *value;
                t_operation = v;
                continue;
            }else if(auto value =std::get_if<std::string>(&i)){
                std::string& v = *value;
                t_reg_num.pop_back( getRegisterName(v));
                continue;
            }else{
                t_value = i;
            }



        }
        switch (t_operation)
        {
        case Opcode::ADD:
            _registers[t_reg_num[0]-1] = _registers[t_reg_num[0]-1] + t_value;
            break;
        
        default:
            break;
        }
        state = states::FINISHED;
    }
   private:
    int id_;   
    Instr intruction; 
    static int _id;
    std::vector<float> _registers; 
    states state;
};
class Warp{
    private:
    static int _id; 
    int id_;
    states state; 

    

    public:
    std::vector<Thread> _threads; 

    Warp(std::vector<Thread> threads): id_(_id++), _threads(threads), state(states::IDLE ){}
    int id() {return _id;}
};
class Program {
public:
    Program(int burst_time, int arrival_time)
        : id_(_id++), burst_time_(burst_time), arrival_time_(arrival_time) {}

    int id()  { return id_; }
    int burstTime() const { return burst_time_; }
    int arrivalTime() const { return arrival_time_; }
    void setBurstTime(int burst_time) { burst_time_ = burst_time; }

private:
     int id_;   
    int burst_time_;
    int arrival_time_;
    static int _id;
};

class GPU{

};

template <typename T>
void remove(std::vector<T>& vec, typename std::vector<T>::iterator pos) {
    vec.erase(pos);
}
class Robin {
public:
    std::vector<Program> readyQueue;
    int quantumTime;

    Robin(std::vector<Program> readyQueue, int quantumTime)
        : readyQueue(std::move(readyQueue)), quantumTime(quantumTime) {}

    void simulate() {
        while (!readyQueue.empty()) {
            for (auto it = readyQueue.begin(); it != readyQueue.end(); ) {
                if (it->burstTime() <= quantumTime) {
                    std::cout << "Program: " << it->id() << " executed to completion\n";
                    it = readyQueue.erase(it);
                } else {
                    it->setBurstTime(it->burstTime() - quantumTime);
                    std::cout << "Program: " << it->id() << " ran, remaining burst = " << it->burstTime() << "\n";
                    ++it;
                }
            }
        }
        std::cout << "ROUND ROBINED!!!\n";
    }
};

int main() {

    Thread t;
    std::vector<Operand> z ={Opcode::ADD, "r2", "r2", 3.2};
    
    Instr x;
    x.op = Opcode::ADD;
    x.src =z;
    t.set_instruction(x);
    
}
