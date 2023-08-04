#include <iostream>
#include <systemc.h>

int flag = 0;

SC_MODULE(stim)
{
    sc_in<bool> Clk;

    void StimGen1()
    {
        cout << "Hello again, world!\n";
        cout << "(1) flag = " << flag << "\n";
        wait();
        flag = 1;
    }
    void StimGen2()
    {
        while(flag != 1){
            wait();
        }
        cout << "(2) flag = " << flag << "\n";
        cout << "Hello World!\n";
    }
    SC_CTOR(stim)
    {
        SC_CTHREAD(StimGen1,Clk.pos());
        SC_CTHREAD(StimGen2,Clk.pos());
    }
};


int sc_main(int argc, char* argv[])
{
    sc_clock TestClk("clk", 10,SC_NS);

    stim Stim1("Stimulus");
    Stim1.Clk(TestClk);

    sc_start();  // run forever

    return 0;

}