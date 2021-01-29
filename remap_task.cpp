/**************************************************************

Remapping a task in memory for CRPD reduction

Author: Thomas Carle (thomas.carle@irit.fr)


**************************************************************/



#include <otawa/proc/Processor.h>

#include <otawa/prog/Inst.h>
#include <otawa/otawa.h>
#include <otawa/cfg/CFG.h>
#include <otawa/app/Application.h>
#include <otawa/cache/features.h>
#include <otawa/hard.h>
#include <otawa/hard/CacheConfiguration.h>
#include "remap_task.h"

#define TRANSLATION_FACTOR 16
#define PRINT_DEBUG 1


using namespace otawa;
using namespace elm;



TranslatedInst::TranslatedInst(Inst* initial_inst, WorkSpace* local_ws, Address new_address):
    VirtualInst(local_ws, initial_inst),
    a(new_address) {};
  
Address TranslatedInst::address(void) const {
    return this->a;
  }

CFGTranslationProcessor::CFGTranslationProcessor(void): CFGTransformer(reg){};
  

Block* CFGTranslationProcessor::transform(Block* currentBlock){
  WorkSpace* local_ws = workspace();
    int translation_factor = TRANSLATION_FACTOR;//For now we translate the address of each instruction of the same factor 
    if(currentBlock->isBasic()){
      BasicBlock* currentBasicBlock = currentBlock->toBasic();
      //Build a table of "translated" instructions, then call the
      //build method to get a new Block.

#ifdef PRINT_DEBUG
      cout << "Initial block starting address: " << currentBasicBlock->address() << "\n";
      cout << "Initial block first inst address: " << currentBasicBlock->first()->address()<<"\n";
#endif
cout << "Initial block address: " << (void*)currentBasicBlock << "\n";
 Inst ** table= new Inst* [currentBasicBlock->count()];
      int table_index=0;
      
      for(BasicBlock::InstIter i = currentBasicBlock->insts(); i; i++){
	
	Address translatedAddr =(i->address() + 16);//translation_factor);//Get the address of the original instruction and apply translation
	//What is the start address of the cache ?
	TranslatedInst* newInst = new TranslatedInst(*i, local_ws,translatedAddr);//Build the translated instruction

	table[table_index++]= newInst;
	
      }
      elm::AllocArray<Inst*> newInstTable ( currentBasicBlock->count(), table);//Polymorphism
      BasicBlock* returned_block = CFGTransformer::build(newInstTable);

#ifdef PRINT_DEBUG
      cout << "Returned block starting address: " << returned_block->address() << "\n";
      cout << "Returned block first inst address: "<< returned_block->first()->address()<< "\n";
#endif
      cout << "Returned block address: " << (void*)returned_block << "\n";
      return returned_block;
    }
    else{
      return CFGTransformer::transform(currentBlock);
    }
}
;


p::declare CFGTranslationProcessor::reg = p::init("otawa::CFGTranslationProcessor", Version(0, 0, 1))
	.maker<CFGTranslationProcessor>()
	      .extend<CFGTransformer>()
	      .require(otawa::VIRTUAL_INST_FEATURE)
	      .require(otawa::hard::CACHE_CONFIGURATION_FEATURE)
  /*.require(hard::MEMORY_FEATURE)
    .require(hard::PROCESSOR_FEATURE)*/;

/*
class MyApp: public Application {
public:
  MyApp(void):
    Application("Instruction re-mapper",Version(1, 0, 0),"Re-mapping basic blocks in memory", "T. Carle")
  {}
protected:
  void work(const string &entry, PropList &props) throw(elm::Exception) {
    try {
      CACHE_CONFIG_PATH(props) = "inst-64x16x1.xml";
      WorkSpace *wsPreemptedTask = workspace();
      require(otawa::COLLECTED_CFG_FEATURE);
      require(otawa::cache::COLLECTED_LBLOCKS_FEATURE);
 
      //require(otawa::LOOP_INFO_FEATURES); //In order to determine if an edge is going out of a loop : if(LOOP_EXIT_EDGE(edge)). Useful for real programs
      
      CFG* toplevelCFG = ENTRY_CFG(wsPreemptedTask);//For now, we consider no
      //subfunction call, so there is
      //only one CFG
      
      CFGTranslationProcessor translationProcessor;
      translationProcessor.process(wsPreemptedTask);
      
    }
    catch(otawa::Exception&e) {
      cerr << "ERROR: " << e.message() << io::endl;
      //return 1;
    }
  }
} ;

OTAWA_RUN(MyApp);
*/
