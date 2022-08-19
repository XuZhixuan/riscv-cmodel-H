#include <iostream>
#include "./ELFParser/ElfParser.hh"
#include "Simulator.hh"

using namespace std;

class Mixed_Model
{
public:
    Mixed_Model(const string image_name, const string config_name, uint64_t initial_pc)
    {
        /* Yaml Config Parser */
        const YAML::Node config = YAML::LoadFile(config_name);
        Emulator::Simulator Simulator(config["Simulator"]);

        /* Elf Parser */
        ELFParser elfloader(image_name);
        Emulator::BaseDRAM *Dram = Simulator.GetDRAM();
        elfloader.Load(Dram);

        // /* Reset Simulator */
        // Simulator.Reset(elfloader.get_entry());
    }
    ~Mixed_Model() {}
    void Evaluate() {}
    void Advance() {}
    uint64_t get_pc() {}
};