#include "BaseDRAM.hh"
namespace Emulator
{
    
BaseDRAM::BaseDRAM(const uint64_t base , const uint64_t length)
: TraceObject("DRAM")
{
    this->m_base = base; 
	this->m_length = length;
	
	this->m_ioBase = 0x0;
	this->m_holeEnd = 0xFFFFFFFF;
	
    this->m_ram  = new char[length];
	this->m_io   = new char[base];
	this->m_hole = new char[this->m_holeEnd - this->m_base - m_length];

	memset(m_ram, 0, length);
	memset(m_io, 0, base);
	memset(m_hole, 0, this->m_holeEnd - this->m_base - m_length);
	
	g_reservationSet = {0,false};	
}
BaseDRAM::~BaseDRAM(){
    delete[] this->m_ram;
    delete[] this->m_io;
	delete[] this->m_hole;
}

void 
BaseDRAM::readHole(uint64_t address, char* data, uint64_t len){
    DASSERT(((address <= m_holeEnd) && (address > (m_base + m_length))),"Access Unknown Address : {:#x}",address);
    //*data = m_ram + (address - m_base);
    memcpy(data, m_hole + address-(m_base + m_length),len);
}   

void 
BaseDRAM::writeHole(uint64_t address,const char* data, const uint64_t len, const uint64_t mask){
    DASSERT(((address <= m_holeEnd) && (address > (m_base + m_length))),"Access Unknown Address : {:#x}",address);
    for(size_t i = 0; i < len; i++){
        if((mask >> i) & 1){
            m_hole[address-(m_base + m_length)+i] = data[i];
        }
    }
}

bool
BaseDRAM::checkHoleRange(uint64_t address){
    return ((address <= m_holeEnd) && (address > (m_base + m_length)));
}

void 
BaseDRAM::readIO(uint64_t address, char* data, uint64_t len){
    DASSERT(((address >= m_ioBase) && (address < m_base)),"Access Unknown Address : {:#x}",address);
    //*data = m_ram + (address - m_base);
    memcpy(data, m_io + address-m_ioBase,len);
}   

void 
BaseDRAM::writeIO(uint64_t address,const char* data, const uint64_t len, const uint64_t mask){
    DASSERT(((address >= m_ioBase) && (address < m_base)),"Access Unknown Address : {:#x}",address);
    for(size_t i = 0; i < len; i++){
        if((mask >> i) & 1){
            m_io[address-m_ioBase+i] = data[i];
        }
    }
}

bool
BaseDRAM::checkIORange(uint64_t address){
    return (address >= m_ioBase) && (address < m_base);
}

void 
BaseDRAM::read(uint64_t address, char** data, uint64_t len){
    DASSERT(((address >= m_base) && (address <= m_base+m_length)),"Access Unknown Address : {:#x}",address);
    *data = m_ram + (address - m_base);
} 

void 
BaseDRAM::read_c(uint64_t address, char* data, uint64_t len){
    DASSERT(((address >= m_base) && (address <= m_base+m_length)),"Access Unknown Address : {:#x}",address);
    //*data = m_ram + (address - m_base);
    memcpy(data, m_ram + address - m_base,len);
}   

bool
BaseDRAM::checkRange(uint64_t address){
    return (address >= m_base) && (address <= m_base+m_length);
}

uint64_t 
BaseDRAM::readDouble(uint64_t address){
    DASSERT(((address >= m_base) && (address <= m_base+m_length)),"Access Unknown Address : {:#x}",address);	
    return *(uint64_t*)(m_ram + (address - m_base)); 
}   

char 
BaseDRAM::readByte(uint64_t address){
    DASSERT(((address >= m_base) && (address <= m_base+m_length)),"Access Unknown Address : {:#x}",address);
    return *(m_ram + (address - m_base)); 
}   

void 
BaseDRAM::write(uint64_t address,const char* data, const uint64_t len){
    DASSERT(((address >= m_base) && (address <= m_base+m_length)),"Access Unknown Address : {:#x}",address);
    std::copy(data,data+len,m_ram + address - m_base);
}

void 
BaseDRAM::writeDouble(uint64_t address,const uint64_t data){
    DASSERT(((address >= m_base) && (address <= m_base+m_length)),"Access Unknown Address : {:#x}",address);
    //std::copy((char*)data, (char*)data+8, m_ram+(address-m_base)); //TODO: whats the problem?
    memcpy(m_ram+(address-m_base), &data, 8);
}

void 
BaseDRAM::write(uint64_t address,const char* data, const uint64_t len, const uint64_t mask){
    DASSERT(((address >= m_base) && (address <= m_base+m_length)),"Access Unknown Address : {:#x}",address);
    for(size_t i = 0; i < len; i++){
        if((mask >> i) & 1){
            m_ram[address-m_base+i] = data[i];
        }
    }
}


const uint64_t& 
BaseDRAM::size(){
    return this->m_length;
}


} // namespace Emulator
