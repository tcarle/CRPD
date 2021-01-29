#include <otawa/prog/VirtualInst.h>
#include <otawa/cfg/CFGTransformer.h>

using namespace otawa;

class TranslatedInst: public VirtualInst {

public:
  TranslatedInst(Inst* initial_inst, WorkSpace* local_ws, Address new_address);
  
  virtual Address address(void) const ;

private:
  Address a;
} ;


class CFGTranslationProcessor: public CFGTransformer {
public:
  static p::declare reg;
  CFGTranslationProcessor(void);
  

protected: 
  virtual Block* transform(Block* currentBlock);
} ;
