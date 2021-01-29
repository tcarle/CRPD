/********************************************************************** 


   Cache Related Preemption Delay computation using OTAWA support

   Author: Thomas Carle (thomas.carle@irit.fr)
   

**********************************************************************/




#include <elm/io.h>
#include <elm/data/Vector.h>
#include <otawa/otawa.h>
#include <otawa/cfg.h>
//#include <otawa/cache/LBlockSet.h>
#include <otawa/cache/features.h>
#include <otawa/hard.h>
#include <otawa/hard/CacheConfiguration.h>
#include <map>
#include <assert.h>
#include <elm/genstruct/HashTable.h>
#include <elm/hash.h>
#include <elm/util/BitVector.h>
#include <stdio.h>
#include <otawa/app/Application.h>
#include "remap_task.h"



#define CACHE_SIZE 32      // 32 blocks of cache. Each block is of size 8 bytes (2 instructions)
#define CACHE_BLOCK_SIZE 8 // Each block is of size 8 bytes (2 instructions)
#define INSTRUCTION_SIZE 4 // 4 bytes instructions

#define BB_NUMBER 100 //To be changed for each preempted application


using namespace elm;
using namespace otawa;
using namespace otawa::cache;

typedef Vector< genstruct::AllocatedTable< LBlock* > > chained_light_cache_configs;
typedef Vector< Block* > block_working_list;

typedef struct NODE node_t;
typedef struct ARC labeled_arc_t;

typedef struct CACHE_STATE_AUTOMATON {
  Vector<node_t*> cache_states;
  Vector<labeled_arc_t*> transitions;
} cache_state_automaton_t;

struct NODE {
  genstruct::AllocatedTable< LBlock* > cache_state;
  Vector<labeled_arc_t*> transitions;
};

struct ARC {
  int label;//corresponds to a basic block identifier
  node_t* destination;
};

bool contains_transition(Vector<labeled_arc_t*> rcs_transitions, labeled_arc_t* new_transition){
  if(rcs_transitions.count()==0){
    return false;
  }

  for(int i = 0; i< rcs_transitions.count(); i++){
    if(rcs_transitions[i]->label==new_transition->label){
      if(rcs_transitions[i]->destination==new_transition->destination){
	return true;
      }
    }
  }

  return false;
}
bool contains_node(Vector<node_t*> rcs_nodes, node_t* new_node){
  
  // cout << "INSIDE CONTAINS_NODE\n";
  if(rcs_nodes.count()==0){
    return false;
  }
  bool found_node= false;
  bool current_node = true;
  for(int i = 0; i< rcs_nodes.count();i++){
    current_node=true;
    //First test the cache_state equality
    if(rcs_nodes[i]->cache_state.count()!=new_node->cache_state.count()){
      current_node = false;continue;
    }
    for(int j = 0; j < new_node->cache_state.count(); j++){
      if(rcs_nodes[i]->cache_state[j]!=new_node->cache_state[j]){
	current_node = false;break;
      }
    }
    //  cout << "\tCACHE_STATES OK\n";
    //Now we check the equality of transitions
    if(rcs_nodes[i]->transitions.count()!=new_node->transitions.count()){
      //    cout << "\t NOT SAME NUMBER OF TRANSITIONS\n";
      current_node = false;continue;
    }
    for(int j = 0; j < rcs_nodes[i]->transitions.count();j++){
      if(rcs_nodes[i]->transitions[j]->label!=new_node->transitions[j]->label){
	current_node = false;break;
      }
      if(rcs_nodes[i]->transitions[j]->destination!=new_node->transitions[j]->destination){
	current_node = false;break;
      }
    }
    if (current_node){
      //   cout << "\tFOUND NODE !!!!\n";
      found_node = true;
      break;
    }
  }
  return found_node;
}


class MyHash {
public:
  static int hash(const node_t& a) {
    int h = 0;
    for(int i = 0; i < a.cache_state.count(); i ++){
      h+=i*(t::intptr)a.cache_state[i];
    }
    if (h>0){
      return h;
    }
    else{
      return -h;
    }
  }
  static bool equals(const node_t& a1, const node_t& a2){
    if(a1.cache_state.count()!=a2.cache_state.count()){
      return false;
    }
    for(int i = 0; i < a1.cache_state.count(); i++){
      if(a1.cache_state[i]!=a2.cache_state[i]){
	return false;
      }
    }
    return true;
  }
};


typedef genstruct::HashTable< node_t, node_t*, MyHash > dico_t;




void compute_output_state(genstruct::AllocatedTable< LBlock* >* p_output_state,
			  genstruct::AllocatedTable< LBlock* >* p_masque,
			  node_t* p_current_state){

  //  cout << "compute_output_state : p_current_state->cache_state.count "<< (p_current_state->cache_state).count()<<"\n";
  // cout << "p_masque size " << p_masque->count() << "\n";
  // cout << "p_output_state size " << p_output_state->count() << "\n";
  // cout << "p_current_state size " << p_current_state->cache_state.count() << "\n";
  for(int i = 0; i<(p_current_state->cache_state).count();i++){
    if((*p_masque)[i]==NULL){
      (*p_output_state)[i]=(p_current_state->cache_state)[i];
      
    }
    else{
      (*p_output_state)[i]=(*p_masque)[i];
    }
    //  cout << "i= " << i << "mask[i] = " << (*p_masque)[i] <<"\n";
    //  cout << "i= " << i << " val = " << (*p_output_state)[i] << "\n";
  }
  // cout << "done\n";
}

bool state_is_contained(node_t* p_node1,node_t* p_node2){
  // genstruct::AllocatedTable< LBlock* > cache_1 = p_node1->cache_state;
  //  genstruct::AllocatedTable< LBlock* > cache_2 = p_node2->cache_state;
  // cout << "(p_node1->cache_state).count()= "<< (p_node1->cache_state).count() << "\n";
  for(int i = 0; i < (p_node1->cache_state).count(); i++){
    if((p_node1->cache_state)[i]!=NULL){
      //  cout << "Non-null case, p1 = " << (p_node1->cache_state)[i] << " p2 = " <<(p_node2->cache_state)[i] << "\n";
      if((p_node1->cache_state)[i]!=(p_node2->cache_state)[i]){
	//	cout << "NOT EQUAL\n";
	return false;
      }
    }
  }

  bool is_equal=true;
  for (int i = 0; i < (p_node1->cache_state).count(); i++){
    if((p_node1->cache_state)[i]!=(p_node2->cache_state)[i]){
      is_equal=false;
    }
  }
  //cout << "STATE EQUAL\n";
  return !is_equal;
}

void compute_RCS_OUT_for_one_bb(Vector< node_t* >* RCS_IN,
				genstruct::AllocatedTable< LBlock* >* p_masque,				
				Block* currentBB,
				Vector< node_t* >* RCS_OUT,
				dico_t* hashtable,
				cache_state_automaton_t* cache_state_automaton,
				int cache_size){
  //We apply existing transitions and add the destination to the
  //RCS_OUT.  If they dont exist, we build the output state of the
  //cache, and check if it already exists in the hashtable. If it
  //does, we add a new transition in the cache states automaton, and
  //we add the output state to the RCS_OUT. If the output state is not
  //in the hashtable, we add it to the automaton with a transition to
  //it and to the hashtable, and we also add the cache state to the
  //RCS_OUT.
  bool transition_found;
  //  cout << "RCS_IN size "<< (*RCS_IN).count() << "\n";
  for(Vector< node_t* >::Iter it_rcs_in(*RCS_IN); it_rcs_in; it_rcs_in++){
    transition_found= false;
    // cout << "New RCS_IN \n";
    if((*RCS_IN)[it_rcs_in]->transitions.count()>0){
      // cout << "(*RCS_IN)[it_rcs_in]->transitions.count()= "<< (*RCS_IN)[it_rcs_in]->transitions.count()<<"\n";
      for(Vector< labeled_arc_t* >::Iter it_trans((*RCS_IN)[it_rcs_in]->transitions);it_trans;it_trans++){
	//cout << &((*RCS_IN)[it_rcs_in]->transitions)[it_trans] << "\n";
	labeled_arc_t* p_current_trans = ((*RCS_IN)[it_rcs_in]->transitions)[it_trans];
	//cout << "p_current_trans->label= "<<p_current_trans->label<< "\n";
	//cout << "((*RCS_IN)[it_rcs_in]->transitions)[it_trans]->label= "<<((*RCS_IN)[it_rcs_in]->transitions)[it_trans]->label<< "\n";
	//cout << "currentBB->id()= "<<currentBB->id()<< "\n";	
	if(p_current_trans->label==currentBB->id()){
	  //cout << "compute_RCS_OUT_for_one_bb: Transition exists\n";
	  //The transition already exists, we check if the destination is already in RCS_OUT. If it is not, we add it

	  //if(!RCS_OUT->contains(p_current_trans->destination)){
	  if(!contains_node(*RCS_OUT,p_current_trans->destination)){
	    //   cout << "Adding destination\n";
	    RCS_OUT->addLast(p_current_trans->destination);
	    // for(int i = 0; i < RCS_OUT->count(); i++){
	    //   cout << "RCS_OUT[i]= " << (*RCS_OUT)[i] << "\n";
	    // }
	  }
	}
	else{
	  //cout << "compute_RCS_OUT_for_one_bb: New transition\n";
	  //the transition does not exist, we must compute the output state, and look for it in the hashtable
	  genstruct::AllocatedTable< LBlock* > output_state (cache_size);
	  node_t *temp_state = new node_t;
	  compute_output_state(&output_state, p_masque, (*RCS_IN)[it_rcs_in]);
	  temp_state->cache_state = output_state;
	  //cout << "Checking Hash\n";
	  //cout << &output_state << "\tcount: " << output_state.count() << "\n";
	  if(hashtable->hasKey(*temp_state)){
	    //    cout << "Checked and found\n";
	    //   cout << "Localisation 1\n";
	    //The state exists in the automaton, we retrieve a pointer to the corresponding state, and add a transition in the automaton
	    node_t* p_corresponding_state = hashtable->get(*temp_state).value();
	    //RCS_OUT->addLast(p_corresponding_state);//We add it to the set of output states
	    labeled_arc_t* new_transition= new labeled_arc_t;
	    new_transition->label=currentBB->id();
	    new_transition->destination= p_corresponding_state;
	    //We addFirst, so our iterator does not take the newly added
	    //transition in consideration. Functionnally it should not
	    //be a problem since we check that the states we try to add
	    //are not already in RCS_IN. But we gain some speed this way
	    //if(!(*RCS_IN)[it_rcs_in]->transitions.contains(new_transition)){
	    if(!contains_transition((*RCS_IN)[it_rcs_in]->transitions,new_transition)){
	      ((*RCS_IN)[it_rcs_in]->transitions).addLast(new_transition);
	    }
	    //We add the new transition to the automaton objects collection for destruction later
	    cache_state_automaton->transitions.addLast(new_transition);
	    //We now add the state to the RCS_OUT

	    //if(!RCS_OUT->contains(p_corresponding_state)){
	    if(!contains_node(*RCS_OUT,p_corresponding_state)){
	      RCS_OUT->addLast(p_corresponding_state);
	    }
	    //   cout << "RCS_OUT size "<< RCS_OUT->count()<<"\n";
	     //   cout << "RCS_IN size "<< (*RCS_IN).count() << "\n";
	     //   cout << "(*RCS_IN)[it_rcs_in]->transitions.count()= "<< (*RCS_IN)[it_rcs_in]->transitions.count()<<"\n";
	  }
	  else{
	    //     cout << "Checked and not found\n";
	     //    cout << "Localisation 2\n";
	    //The state does not exist in the automaton, we must add it
	    node_t* new_node = new node_t;
	    genstruct::AllocatedTable< LBlock* > new_node_cache_state(cache_size);
	    new_node->cache_state = new_node_cache_state;
	    for(int i = 0;i< output_state.count();i++){
	      //cout << "output_state not empty\n";
	      (new_node->cache_state)[i]=output_state[i];
	    }
	    labeled_arc_t* new_transition= new labeled_arc_t;
	    new_transition->label=currentBB->id();
	    new_transition->destination=new_node;

	    //if(!(*RCS_IN)[it_rcs_in]->transitions.contains(new_transition)){
	    if(!contains_transition((*RCS_IN)[it_rcs_in]->transitions,new_transition)){
	      ((*RCS_IN)[it_rcs_in]->transitions).addLast(new_transition);
	    }
	    //We must also add an entry to the hashtable
	    //cout << &(new_node->cache_state) <<"\n";
	    hashtable->put(*new_node,new_node);
	    //We add the node and transition to the cache state automaton objects for desctruction later
	    cache_state_automaton->cache_states.addLast(new_node);
	    cache_state_automaton->transitions.addLast(new_transition);
	    //Now we also add the new state to the RCS_OUT

	    //if(!RCS_OUT->contains(new_node)){
	    if(!contains_node(*RCS_OUT,new_node)){
	      RCS_OUT->addLast(new_node);
	    }
	    //cout << "RCS_OUT size "<< RCS_OUT->count()<<"\n";
	  }
	}
      }
    }
    else{
      //No transition yet, we must create one
      //  cout << "compute_RCS_OUT_for_one_bb: New transition2\n";
	  //the transition does not exist, we must compute the output state, and look for it in the hashtable
      genstruct::AllocatedTable< LBlock* >  output_state (cache_size);
      node_t * temp_state = new node_t;
      compute_output_state(&output_state, p_masque, (*RCS_IN)[it_rcs_in]);
      temp_state->cache_state = output_state;
      
      //   cout << "Checking Hash2\n";
      //cout << &output_state << "\tcount: " << output_state.count() << "\n";
      //  cout << "hashtable size = " << hashtable->count() << "\n";
      if(hashtable->hasKey(*temp_state)){
	//	cout << "Checked and found2\n";
	//cout << "Already existing state\n";
	//The state exists in the automaton, we retrieve a pointer to the corresponding state, and add a transition in the automaton
	node_t* p_corresponding_state = hashtable->get(*temp_state).value();
	//RCS_OUT->addLast(p_corresponding_state);//We add it to the set of output states
	labeled_arc_t* new_transition= new labeled_arc_t;
	new_transition->label=currentBB->id();
	new_transition->destination= p_corresponding_state;
	//We addFirst, so our iterator does not take the newly added
	//transition in consideration. Functionnally it should not
	//be a problem since we check that the states we try to add
	//are not already in RCS_IN. But we gain some speed this way

	//if(!((*RCS_IN)[it_rcs_in]->transitions).contains(new_transition)){
	if(!contains_transition((*RCS_IN)[it_rcs_in]->transitions,new_transition)){
	  ((*RCS_IN)[it_rcs_in]->transitions).addLast(new_transition);
	}
	//We add the new transition to the automaton objects collection for destruction later
	cache_state_automaton->transitions.addLast(new_transition);
	//We now add the state to the RCS_OUT
	//if(!RCS_OUT->contains(p_corresponding_state)){
	if(!contains_node(*RCS_OUT,p_corresponding_state)){
	  RCS_OUT->addLast(p_corresponding_state);
	}
      }
      else{
	//	cout << "Checked2-2\n";
	//cout << "Non-already existing state\n";
	//The state does not exist in the automaton, we must add it
	node_t* new_node = new node_t;
	genstruct::AllocatedTable< LBlock* > new_node_cache_state(cache_size);
	new_node->cache_state = new_node_cache_state;
	for(int i = 0;i< output_state.count();i++){
	  //cout << "output_state not empty\n";
	  (new_node->cache_state)[i]=output_state[i];
	}
	labeled_arc_t* new_transition= new labeled_arc_t;
	new_transition->label=currentBB->id();
	new_transition->destination=new_node;
	
	//if(!((*RCS_IN)[it_rcs_in]->transitions).contains(new_transition)){
	if(!contains_transition((*RCS_IN)[it_rcs_in]->transitions,new_transition)){
	  ((*RCS_IN)[it_rcs_in]->transitions).addLast(new_transition);
	}
	
	//We must also add an entry to the hashtable
	//cout << &(new_node->cache_state)<< "\n";
	hashtable->put(*new_node,new_node);
	//We add the node and transition to the cache state automaton objects for desctruction later
	cache_state_automaton->cache_states.addLast(new_node);
	cache_state_automaton->transitions.addLast(new_transition);
	//Now we also add the new state to the RCS_OUT
	//if(!RCS_OUT->contains(new_node)){
	//	cout << "RCS_OUT size= " << RCS_OUT->count() << "\n";
	if(!contains_node(*RCS_OUT,new_node)){
	  //	  cout << "Adding node to RCS_OUT\n";
	  RCS_OUT->addLast(new_node);
	}
	//cout << "RCS_OUT size "<< RCS_OUT->count()<<"\n";
      }
    }
    //  cout << "number of transitions " <<((*RCS_IN)[it_rcs_in]->transitions).count() <<"\n";
    //int cpt = RCS_OUT->count();
//cout << "RCS_OUT size : " << RCS_OUT->count() <<"\n";
  char to_remove[RCS_OUT->count()];
  int it=0;
  //cout << "RCS_OUT->count()= "<< RCS_OUT->count()<<"\n";
  for(int it_rcs_out=0; it_rcs_out< RCS_OUT->count(); it_rcs_out++){
    /*Vector<node_t*>::Iter j = it_rcs_out;//We declare a second iterator*/
    to_remove[it]=0;
    //j++;//We set it just after it_rcs_out, so we wont "compare" an element with itself
    for(int j=0;j<RCS_OUT->count();j++){
      // cout << "DEBUG INSIDE\\\\\\\\\\\\\n";
      if(j!=it_rcs_out){
	//	cout << "DEBUG INSIDE\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\n";
	if(state_is_contained((*RCS_OUT)[it_rcs_out],(*RCS_OUT)[j])){
	  to_remove[it]=1;
	  break;
	}
      }
    }
    it++;
  }
  //  cout << "DEBUG-------2---- count =  "<< RCS_OUT->count() << "\n";
  int shift=0;
  for(it=0;it<RCS_OUT->count();it++){
    if(to_remove[it]==1){
      RCS_OUT->removeAt(it+shift);
      shift-=1;
      //  cout << "Removing state\n";
    }
  }
  
  }
  //Now we go through the RCS_OUT and keep only the "maximal" cache
  //configurations, i.e. the ones that are not contained in other
  //cache configurations
  //cout<< "DEBUG!!!!!!\n";
  // int cpt = RCS_OUT->count();
  // cout << "RCS_OUT size : " << RCS_OUT->count() <<"\n";
  // char to_remove[RCS_OUT->count()];
  // int it=0;
  // //cout << "RCS_OUT->count()= "<< RCS_OUT->count()<<"\n";
  // for(int it_rcs_out=0; it_rcs_out< RCS_OUT->count(); it_rcs_out++){
  //   /*Vector<node_t*>::Iter j = it_rcs_out;//We declare a second iterator*/
  //   to_remove[it]=0;
  //   //j++;//We set it just after it_rcs_out, so we wont "compare" an element with itself
  //   for(int j=0;j<RCS_OUT->count();j++){
  //     // cout << "DEBUG INSIDE\\\\\\\\\\\\\n";
  //     if(j!=it_rcs_out){
  // 	//	cout << "DEBUG INSIDE\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\n";
  // 	if(state_is_contained((*RCS_OUT)[it_rcs_out],(*RCS_OUT)[j])){
  // 	  to_remove[it]=1;
  // 	  break;
  // 	}
  //     }
  //   }
  //   it++;
  // }
  // //  cout << "DEBUG-------2---- count =  "<< RCS_OUT->count() << "\n";
  // int shift=0;
  // for(it=0;it<RCS_OUT->count();it++){
  //   if(to_remove[it]==1){
  //     RCS_OUT->removeAt(it+shift);
  //     shift-=1;
  //     //  cout << "Removing state\n";
  //   }
  // }
  //  cout << "DEBUG------------3\n";
  // cout << "RCS_OUT[local] size: "<< RCS_OUT->count() << "\n";
}


void print_RCS_OUT(Vector<node_t*>  RCS_OUT, int block_id, int cache_size){
  // cout << "Block " << block_id << "\n";
    for(Vector<node_t*>::Iter it_rcs_out(RCS_OUT); it_rcs_out; it_rcs_out++){
      //genstruct::AllocatedTable<LBlock*> cache_state = RCS_OUT[it_rcs_out]->cache_state;
      for(int j = 0; j < cache_size; j++){
	if(RCS_OUT[it_rcs_out]->cache_state[j]!=NULL){
	  //	  cout<< "\t" << RCS_OUT[it_rcs_out]->cache_state[j]->address()<<"\n";
	}
	else{
	  //	  cout << "\t" << 0x0 << "\n";
	}
      }
      //   cout << "Next\n";
    }
  
}


bool compare_sets(Vector<node_t*>* set1, Vector<node_t*>* set2){
  // cout << "\tcomparing sets :";
  bool still_equal = (set1->count()==set2->count());//First we check that both vectors have the same length
  // cout << "same size : " << still_equal << "\n";

  // cout << "set1\n";
  for(int i =0 ; i < set1->count(); i++){
    //  cout << (*set1)[i] <<"\n";
  }
  // cout << "set2\n";
  for(int i =0 ; i < set2->count(); i++){
    //   cout << (*set2)[i] <<"\n";
  }
  
  for(int i = 0; i < set1->count(); i++){
    if(!set2->contains((*set1)[i])){
      still_equal=false;
      break;
    }
  }
  return still_equal;
}


//TODO: definir une classe heritant de la classe Application, et
//surcharger la methode work avec le code du main. + ajouter un
//booleen pour faire des choses differentes selon qu'on appelle la
//premiere fois ou la deuxieme fois.
class MyApp: public Application {
public:
  MyApp(void):
    Application("CRPD scanner",Version(2, 0, 0),"Counting the CRPD", "T. Carle")
  {
    this->first_time=true;
  }
protected:
  void work(const string &entry, PropList &props) throw(elm::Exception) {
    try {
      if(this->first_time){
	
	CACHE_CONFIG_PATH(props) = "inst-64x16x1.xml";
	WorkSpace *wsPreemptedTask = workspace();
	require(otawa::COLLECTED_CFG_FEATURE);
	require(otawa::cache::COLLECTED_LBLOCKS_FEATURE);
	//require(otawa::LOOP_INFO_FEATURES); //In order to determine if an edge is going out of a loop : if(LOOP_EXIT_EDGE(edge)). Useful for real programs

	
	CFG* toplevelCFG = ENTRY_CFG(wsPreemptedTask);//For now, we consider no
				      //subfunction call, so there is
				      //only one CFG
    
    
    
    

	const hard::CacheConfiguration * cache_config = hard::CACHE_CONFIGURATION(wsPreemptedTask);
	const hard::Cache* icache = cache_config->instCache();
	this->cache_size = icache->cacheSize()/8;//Lines have 2 instructions of four bytes--changes with each configuration // /16 for small example with 4 words per line
	LBlockSet** mapped_cache = LBLOCKS(wsPreemptedTask);
	int bb_number = toplevelCFG->count();//there is an entry and an exit blocks that correspond to no code
	this->bb_number=bb_number;
	Vector<node_t*> RCS_IN[bb_number];
	Vector<node_t*> RCS_OUT[bb_number];
	Vector<node_t*> RCS_OUT_pre;
	
	Vector<node_t*> LCS_IN[bb_number];
	Vector<node_t*> LCS_IN_pre;
	Vector<node_t*> LCS_OUT[bb_number];
	
	printf("Setting mask pointers to NULL : \n");
	printf("bb_number: %i\n", bb_number);
	printf("cache_size: %i\n",this->cache_size);
	genstruct::AllocatedTable<LBlock*> masque[bb_number];
	for (int i=0;i<bb_number;i++){
	  RCS_IN[i].clear();
	  RCS_OUT[i].clear();
	  LCS_IN[i].clear();
	  LCS_OUT[i].clear();
	  masque[i].allocate(this->cache_size);
	  for (int j=0;j<this->cache_size;j++){
	    masque[i][j]=NULL;
	  }
	}
	printf("done\n");
	//hard::Cache clean_cache = hard::Cache(*icache);
	// CFGCollection* CFGcoll = INVOLVED_CFGS(ws1);//Maybe later
	
	dico_t* hashtable_rcs= new dico_t;
	cache_state_automaton_t* cache_state_automaton_rcs = new cache_state_automaton_t;
	
	dico_t* hashtable_lcs = new dico_t;
	cache_state_automaton_t* cache_state_automaton_lcs = new cache_state_automaton_t;     
	
	Block* entryBlock= toplevelCFG->entry();//entryBlock is not a real block of the CFG- no instruction
	// Block* currentBlock = (entryBlock->outs())[0].sink();
	
	//std::map<int,int> redirect;
	
	int current_entry=0;
	//We initialize the masks i.e. genB for each basic block B, and we add them to the RCS_OUT and the automaton
	printf("Initializing the masks\n");
	for (CFG::BlockIter i=toplevelCFG->blocks();i;i++ ){
	  Block* currentBlock = i;
	  int currentBlockId = currentBlock->id();
	  cout << "Block $" << currentBlockId << "current entry = "<< current_entry<<"\n";
	  if(currentBlock->isBasic()){
	    //currentBlock->toBasic(); 
	    //genstruct::AllocatedTable< LBlock* >* bblblocks = BB_LBLOCKS(*currentBlock);//TODO: quand une ligne du cache na pas de lblock correspondant, est ce que cette ligne est a NULL ?
	    elm::AllocArray< LBlock* >* bblblocks = BB_LBLOCKS(*currentBlock);
	    int table_size=bblblocks->count();
	    this->redirect[currentBlockId]=current_entry;
	    for(int j=0;j<table_size;j++){
	      LBlock* current_lblock = (*bblblocks)[j];  
	      masque[current_entry][icache->set(current_lblock->address())]=current_lblock;
	      cout << icache->set(current_lblock->address()) << "\n";
	    }
	    node_t* mask_node = new node_t;
	    mask_node->cache_state=masque[current_entry];
	    //Set up for RCS computation
	    (cache_state_automaton_rcs->cache_states).addLast(mask_node);
	    cout << &(mask_node->cache_state) <<"\n";
	    //hashtable_rcs->put(*mask_node,mask_node);
	    RCS_OUT[current_entry].addLast(mask_node);
	    //Set up for LCS computation
	    (cache_state_automaton_lcs->cache_states).addLast(mask_node);
	    //hashtable_lcs->put(*mask_node,mask_node);
	    LCS_IN[current_entry].addLast(mask_node);
	    current_entry++;
	  }
	}
	printf("Done\n");
	
	//Now we can compute the RCS
	printf("Computing the RCS\n");
	block_working_list* wl = new block_working_list;
	//wl->add((entryBlock->outs())[0].sink());//
	wl->add(entryBlock);
	Block* currentBlock;
	while (!(wl->isEmpty())){

	  cout << "\n\nNEW ITERATION\n\n";
	  cout << "wl size: "<< wl->count()<<"\n";
	  
	  currentBlock = wl->pop();
	  if(currentBlock->isBasic()){

	    cout << "current block : " << currentBlock->id() << "\n";
	    //currentBlock->toBasic();//Not sure if I need it yet
	    RCS_IN[this->redirect[currentBlock->id()]].clear();//We clean the RCS_IN before recomputing it
	    for(Block::EdgeIter i = currentBlock->ins();i;i++){
	      //Compute the set of RCS_IN
	      cout << "Working on source " << i->source()->id() << "\n";
	      cout << "RCS_OUT_SIZE= "<< RCS_OUT[this->redirect[i->source()->id()]].count() << "\n";
	      for(Vector<node_t*>::Iter it_rcs_out(RCS_OUT[this->redirect[i->source()->id()]]);it_rcs_out;it_rcs_out++){
		//For each predecessor, we copy its RCS_OUT in the RCS_IN of the current block
		if((i->source()->isBasic())){
		  if(!(RCS_IN[this->redirect[currentBlock->id()]].contains(RCS_OUT[this->redirect[i->source()->id()]][it_rcs_out]))){
		    RCS_IN[this->redirect[currentBlock->id()]].addLast(RCS_OUT[this->redirect[i->source()->id()]][it_rcs_out]);
		  }
		}
	      }
	    }
	    
	    RCS_OUT_pre = RCS_OUT[this->redirect[currentBlock->id()]];
	    cout << "Printing RCS_OUT_pre\n";
	    print_RCS_OUT(RCS_OUT_pre,currentBlock->id(),this->cache_size);
	    cout << "Done printing\n";
	    RCS_OUT[this->redirect[currentBlock->id()]].clear();
	    //cout << "\t calling compute_RCS_OOT_for_one_bb\n";
	    compute_RCS_OUT_for_one_bb(
				       &(RCS_IN[this->redirect[currentBlock->id()]]),
				       &masque[this->redirect[currentBlock->id()]],
				       currentBlock,
				       &(RCS_OUT[this->redirect[currentBlock->id()]]),
				       hashtable_rcs,
				       cache_state_automaton_rcs,
				       this->cache_size);
	    cout << "Printing RCS_OUT\n";
	    print_RCS_OUT(RCS_OUT[this->redirect[currentBlock->id()]],currentBlock->id(),this->cache_size);
	    cout << "Done printing\n";

	    //if(RCS_OUT_pre!=RCS_OUT[this->redirect[currentBlock->id()]]){
	    if(!(compare_sets(&RCS_OUT_pre,&(RCS_OUT[this->redirect[currentBlock->id()]])))){
	      cout << "SETS UNEQUAL\n";
	      for(Block::EdgeIter i = currentBlock->outs();i;i++){
		if(!wl->contains(i->sink())){
		  wl->addLast(i->sink());
		}
	      }
	    }
	  }
	  else{
	    cout << "NOT A BASIC BLOCK\n";
	    for(Block::EdgeIter i = currentBlock->outs();i;i++){
	      if(!wl->contains(i->sink())){
		wl->addLast(i->sink());
	      }
	    }
	  }
	}
	printf("Done\n");

	//Now we compute the LCS
	printf("Computing LCS\n");
	Block* sinkBlock = toplevelCFG->exit()->ins()[0].source();
	//Block* sinkBlock = toplevelCFG->entry()->outs()[0].sink();//For now we use this. To be modified for the general case.
	//sinkBlock=sinkBlock->ins()[0].source();
	wl->add(sinkBlock);//Before that wl is guaranteed to be empty
	while(!(wl->isEmpty())){
	  currentBlock= wl->pop();
	  if(currentBlock->isBasic()){
	    //cout<<"INSIDE LCS\n";
	    //currentBlock->toBasic();
	    LCS_OUT[this->redirect[currentBlock->id()]].clear();
	    for(Block::EdgeIter i = currentBlock->outs();i;i++){
	      for(Vector<node_t*>::Iter it_lcs_in(LCS_IN[this->redirect[i->sink()->id()]]); it_lcs_in; it_lcs_in++){
		if(i->sink()->isBasic()){
		  LCS_OUT[this->redirect[currentBlock->id()]].addLast(LCS_IN[this->redirect[i->sink()->id()]][it_lcs_in]);
		}
	      }
	    }
	    //cout <<"Printing LCS_OUT\n";
	    //print_RCS_OUT(LCS_OUT[this->redirect[currentBlock->id()]],currentBlock->id(),this->cache_size);
	    LCS_IN_pre = LCS_IN[this->redirect[currentBlock->id()]];
	    LCS_IN[this->redirect[currentBlock->id()]].clear();
	    //compute_RCS_OUT_for_one_bb can be used to compute LCN_IN
	    compute_RCS_OUT_for_one_bb(
				       &(LCS_OUT[this->redirect[currentBlock->id()]]),
				       &masque[this->redirect[currentBlock->id()]],
				       currentBlock,
				       &(LCS_IN[this->redirect[currentBlock->id()]]),
				       hashtable_lcs,
				       cache_state_automaton_lcs,
				       this->cache_size);
	    //cout <<"Printing LCS_IN\n";
	    //print_RCS_OUT(LCS_IN[this->redirect[currentBlock->id()]],currentBlock->id(),this->cache_size);

	    //if(LCS_IN_pre!=LCS_IN[this->redirect[currentBlock->id()]]){
	    if(!(compare_sets(&LCS_IN_pre,&(LCS_IN[this->redirect[currentBlock->id()]])))){
	      for(Block::EdgeIter i =currentBlock->ins();i;i++){
		if(!wl->contains(i->source())){
		  wl->addLast(i->source());
		}
	      }
	    }
	  }
	  else{
	    for(Block::EdgeIter i =currentBlock->ins();i;i++){
	      if(i->source()->isBasic()){
		if(!wl->contains(i->source())){
		  wl->addLast(i->source());
		}
	      }
	    }
	  }
	}
	printf("Done\n");
	//printf("PRINTING THE LCS\n" );
	//for(int i = 0; i<bb_number; i++){
	//  print_RCS_OUT(LCS_OUT[this->redirect[i]],i,this->cache_size);
	//}



	
	//Now we compute the Cache Utility Vectors for each block
	printf("Computing CUV\n");
	//Vector<BitVector*> CUV[bb_number];
	for(int i=0;i<bb_number;i++){//For each basic block
	  //cout << "BB $" << i <<"\n";
	  for(Vector<node_t*>::Iter it_rcs_out(RCS_OUT[i]);it_rcs_out;it_rcs_out++){//for each configuration of the RCS_OUT of the bb
	    for(Vector<node_t*>::Iter it_lcs_in(LCS_OUT[i]);it_lcs_in;it_lcs_in++){//for each configuration of the LCS_IN of the bb
	      BitVector* local_cuv=new BitVector(this->cache_size);//Static declaration - prohibits use in functions, but no need to keep track for destruction
	      for(int j = 0; j<this->cache_size;j++){
		if(RCS_OUT[i][it_rcs_out]->cache_state[j]==LCS_OUT[i][it_lcs_in]->cache_state[j]){
		  local_cuv->set(j);
		  // cout << "1";
		}
		else{
		  //  cout << "0";
		}
	      }
	      //cout <<"\n";
	      this->CUV[i].addLast(local_cuv);
	    }
	  }
	}
	printf("Done\n");
	this->first_time=false;
	
      }
      else{
	//We also must compute iteratively the RCS of the preempting task
	printf("Now with the preempting task\n");

	//Getting the workspace for the new run of the work function
	workspace()->invalidate(otawa::COLLECTED_CFG_FEATURE);
	require(otawa::COLLECTED_CFG_FEATURE);

	
	PropList props_preempting;
	Processor::VERBOSE(props_preempting) = true; 
	CACHE_CONFIG_PATH(props_preempting) = "inst-64x16x1.xml";
	
	
	WorkSpace *wsPreemptingTask = workspace();
	require(otawa::COLLECTED_CFG_FEATURE);//This is invalidated and re-required directly using the CFGTransformer

	//for(int iteration = 0; iteration< 1; iteration++){
	//  if(iteration!=0){
	    

	    //We use the last found conflicting block, and try to optimize the CRPD for this block
	    

	    
	//   CFGTranslationProcessor* translationProcessor = new CFGTranslationProcessor();//Problem: it always starts with the initial CFG, not the one we already processed
	//   translationProcessor->process(wsPreemptingTask,props_preempting);
	    
	//   cout << "\n\tws->isAvailable: "<< wsPreemptingTask->isProvided(otawa::COLLECTED_CFG_FEATURE)<<"\n";

	   
	//	  }
	  //cout << "Processing done\n";
	  
	  require(otawa::cache::COLLECTED_LBLOCKS_FEATURE);//Must be required everytime we modify the CFG using the CFGTransformer class
	  //cout << "Just required\n";

	  
	  CFG* toplevelCFGpreempting = ENTRY_CFG(wsPreemptingTask);//For now, we consider no
	  //cout << "\n\tCFG address= " << (void*) toplevelCFGpreempting << "\n";
	  //subfunction call, so there is
	  //only one CFG
	  
	  LBlockSet** mapped_cache_preempting = LBLOCKS(wsPreemptingTask);
	  const hard::CacheConfiguration* cache_config_preempting = hard::CACHE_CONFIGURATION(wsPreemptingTask);
	  
	  const hard::Cache* icache_preempting = cache_config_preempting->instCache();
	  int cache_size_preempting = this->cache_size;
	  
	  int bb_number_preempting = toplevelCFGpreempting->count();//there is an entry and an exit blocks that correspond to no code
	  //cout << "bb_number_preempting = " << bb_number_preempting << "\n";
	  Vector<node_t*> RCS_IN_preempting[bb_number_preempting];
	  Vector<node_t*> RCS_OUT_preempting[bb_number_preempting];
	  Vector<node_t*> RCS_OUT_pre_preempting;
	  
	  genstruct::AllocatedTable<LBlock*> masque_preempting[bb_number_preempting];
	  for (int i=0;i<bb_number_preempting;i++){
	    RCS_IN_preempting[i].clear();
	    RCS_OUT_preempting[i].clear();
	    masque_preempting[i].allocate(cache_size_preempting);
	    for (int j=0;j<cache_size_preempting;j++){
	      masque_preempting[i][j]=NULL;
	    }
	  }
	  
	  dico_t* hashtable_rcs_preempting= new dico_t;
	  cache_state_automaton_t* cache_state_automaton_rcs_preempting = new cache_state_automaton_t;
	
	  dico_t* hashtable_lcs_preempting = new dico_t;
	  cache_state_automaton_t* cache_state_automaton_lcs_preempting = new cache_state_automaton_t;
	
	  Block* entryBlock_preempting= toplevelCFGpreempting->entry();//entryBlock is not a real block of the CFG- no instruction
	  // Block* currentBlock = (entryBlock->outs())[0].sink();
	
	  std::map<int,int> redirect_preempting;
	  int current_entry_preempting=0;
	  //We initialize the masks i.e. genB for each basic block B, and we add them to the RCS_OUT and the automaton
	  
	  //cout << "Initializing the masks\n";
	  for (CFG::BlockIter i=toplevelCFGpreempting->blocks();i;i++ ){
	    Block* currentBlock = i;
	    int currentBlockId = currentBlock->id();
	    if(currentBlock->isBasic()){
	      //BasicBlock * currentBasicBlock = currentBlock->toBasic();
	      cout << "Block address: "<< (void*)i<<"\n";
	      //genstruct::AllocatedTable< LBlock* >* bblblocks = BB_LBLOCKS(*currentBlock);//TODO: quand une ligne du cache na pas de lblock correspondant, est ce que cette ligne est a NULL ?
	      elm::AllocArray< LBlock* >* bblblocks = BB_LBLOCKS(*currentBlock);//TODO: quand une ligne du cache na pas de lblock correspondant, est ce que cette ligne est a NULL ?
	      int table_size=bblblocks->count();
	      redirect_preempting[currentBlockId]=current_entry_preempting;
	      for(int j=0;j<table_size;j++){
		LBlock* current_lblock = (*bblblocks)[j];
		//int maps_to = current_lblock->lblockset()->set();
		
		masque_preempting[current_entry_preempting][icache_preempting->set(current_lblock->address())]=current_lblock;
	  
	      }
	      node_t* mask_node_preempting = new node_t;
	      mask_node_preempting->cache_state=masque_preempting[current_entry_preempting];
	      //Set up for RCS computation
	      (cache_state_automaton_rcs_preempting->cache_states).addLast(mask_node_preempting);
	      hashtable_rcs_preempting->put(*mask_node_preempting,mask_node_preempting);
	      RCS_OUT_preempting[current_entry_preempting].addLast(mask_node_preempting);
	      current_entry_preempting++;
	    }
	  }
	  // printf("Done\n");
	
	  //Now we can compute the RCS
	  block_working_list* wl_preempting = new block_working_list;
	  wl_preempting->add(entryBlock_preempting);
	  Block* currentBlock_preempting;
	  //printf("Computing RCS\n");
	  while (!(wl_preempting->isEmpty())){
	    currentBlock_preempting = wl_preempting->pop();
	    if(currentBlock_preempting->isBasic()){
	      //currentBlock_preempting->toBasic();//Not sure if I need it yet
	      RCS_IN_preempting[currentBlock_preempting->id()].clear();//We clean the RCS_IN before recomputing it
	      for(Block::EdgeIter i = currentBlock_preempting->ins();i;i++){
		//Compute the set of RCS_IN
		for(Vector<node_t*>::Iter it_rcs_out(RCS_OUT_preempting[redirect_preempting[i->source()->id()]]);it_rcs_out;it_rcs_out++){
		  //For each predecessor, we copy its RCS_OUT in the RCS_IN of the current block
		  if(i->source()->isBasic()){
		    RCS_IN_preempting[redirect_preempting[currentBlock_preempting->id()]].addLast(RCS_OUT_preempting[redirect_preempting[i->source()->id()]][it_rcs_out]);
		  }
		}
	      }
	      RCS_OUT_pre_preempting = RCS_OUT_preempting[redirect_preempting[currentBlock_preempting->id()]];
	      RCS_OUT_preempting[redirect_preempting[currentBlock_preempting->id()]].clear();
	      compute_RCS_OUT_for_one_bb(
					 &(RCS_IN_preempting[redirect_preempting[currentBlock_preempting->id()]]),
					 &masque_preempting[redirect_preempting[currentBlock_preempting->id()]],
					 currentBlock_preempting,
					 &(RCS_OUT_preempting[redirect_preempting[currentBlock_preempting->id()]]),
					 hashtable_rcs_preempting,
					 cache_state_automaton_rcs_preempting,
					 this->cache_size);

	      //if(RCS_OUT_pre_preempting!=RCS_OUT_preempting[redirect_preempting[currentBlock_preempting->id()]]){
	      if(!(compare_sets(&RCS_OUT_pre_preempting,&(RCS_OUT_preempting[redirect_preempting[currentBlock_preempting->id()]])))){
		for(Block::EdgeIter i = currentBlock_preempting->outs();i;i++){
		  if(!wl_preempting->contains(i->sink())){
		    wl_preempting->addLast(i->sink());
		  }
		}
	      }
	    }
	    else{
	      for(Block::EdgeIter i = currentBlock_preempting->outs();i;i++){
		if(!wl_preempting->contains(i->sink())){
		  wl_preempting->addLast(i->sink());
		}
	      }
	    }
	  }
	  	  
	  //Now we create a bitvector that represents the FUV of the preempting task
	  //First, we must find the id of the last basic block of the task
	  Block* sinkBlock_preempting = toplevelCFGpreempting->exit();
	  int last_block_id_preempting;
	  for(Block::EdgeIter i = sinkBlock_preempting->ins();i;i++){//"There can be only one"-- Connor MacLeod
	    last_block_id_preempting = i->source()->id();
	  }
	  Vector<node_t*> selected_preempting_RCS_OUT = RCS_OUT_preempting[redirect_preempting[last_block_id_preempting]];
	  
	  Vector<BitVector*> FUV_collection;
	  //printf("Computing the FUV collection\n");
	  for(Vector<node_t*>::Iter it_rcs_out(selected_preempting_RCS_OUT);it_rcs_out;it_rcs_out++){
	    BitVector * FUV = new BitVector(cache_size_preempting);
	    for(int i=0;i<cache_size_preempting;i++){
	      if(selected_preempting_RCS_OUT[it_rcs_out]->cache_state[i]!=NULL){
	  	FUV->set(i);
	      }
	    }
	    FUV_collection.addLast(FUV);
	  }
	  // printf("Done\n");
	  //This is the final phase. We do not only want to obtain the
	  //maximum delay, but information regarding this delay: in
	  //particular, which basic blocks are involved.

	  
	  Vector<BitVector*> Delay[this->bb_number];
	  int crpd=0;
	  int conflicting_BB=-1;
	  int theoretical_min_interference=0;
	  //BitVector * clashing_fuv = new BitVector(cache_size_preempting);
	  //BitVector * clashing_cuv=new BitVector(cache_size_preempting);
	  int best_crpd =1000;
	  int best_shift = 0;
	  bool shifted = false;
	  for(int k=0;k<(*FUV_collection[0]).size();k++){

	    for(int i=0; i<this->bb_number;i++){
	      //cout << "i= " << i << "\n";
	      for(Vector<BitVector*>::Iter it_cuv(this->CUV[i]);it_cuv;it_cuv++){
		for(Vector<BitVector*>::Iter it_fuv(FUV_collection);it_fuv;it_fuv++){
		  if(this->cache_size - this->CUV[i][it_cuv]->countBits() - (*FUV_collection[it_fuv]).countBits()<theoretical_min_interference){
		    theoretical_min_interference = this->cache_size - this->CUV[i][it_cuv]->countBits() - (*FUV_collection[it_fuv]).countBits();
		  }
		  //cout << "Theoretical min interference: "<< this->cache_size - this->CUV[i][it_cuv]->countBits() - (*FUV_collection[it_fuv]).countBits()<< "\n";
		  //	cout << *FUV_collection[it_fuv] << "\n";
		
		  //Shift the FUV by 1 to the left
		  if(!shifted){
		    BitVector * shifted_fuv = new BitVector(cache_size_preempting);
		  
		    for(int p=1;p<cache_size_preempting;p++){
		      if((*FUV_collection[it_fuv]).bit(p)){
			shifted_fuv->set(p-1);
		      }
		    }
		    if((*FUV_collection[it_fuv]).bit(0)){
		      shifted_fuv->set(cache_size_preempting-1);
		    }
		    FUV_collection[it_fuv]=shifted_fuv;
		    shifted=true;
		  }
		  BitVector conjunction = this->CUV[i][it_cuv]->makeAnd(*FUV_collection[it_fuv]);
		  Delay[i].addLast(&conjunction);
		  if(crpd < conjunction.countBits()){
		    crpd = conjunction.countBits();
		    conflicting_BB = i;
		  
		  }
		  //	cout << "FUV\n";
		  //	cout << *FUV_collection[it_fuv] << "\n";
		}
	      
		//  cout << "CUV\n";
		//  cout << *(this->CUV[i][it_cuv]) <<"\n";
	      }
	    }
	    if(crpd<best_crpd){
	      best_crpd=crpd;
	      best_shift=k+1;
	    }
	    printf("CRPD: %d\n", crpd);
	    crpd=0;
	    shifted=false;
	  
	  }
	  cout<<"\n\nBEST CRPD: "<< best_crpd << " , corresponding shift: " << best_shift << "\n";
	  printf("Theoretical minimum interference: %d\n", theoretical_min_interference);
	  if(best_crpd == -theoretical_min_interference){
	    printf("Optimum reached\n");
	  }
	  if(conflicting_BB!=-1){
	    for(std::map<int,int>::iterator it=this->redirect.begin();it!=this->redirect.end(); it++){
	      if (it->second==conflicting_BB){
		printf("Conflicting BB: %i\n", it->first);
	      }
	    }
	  }
	  this->last_conflicting_BB=conflicting_BB;
	  //}//for
      }
      //return 0;
    }
    catch(otawa::Exception&e) {
      cerr << "ERROR: " << e.message() << io::endl;
      //return 1;
    }
  }
private:
  bool first_time;
  Vector<BitVector*> CUV[BB_NUMBER];
  int bb_number;
  std::map<int,int> redirect;
  int cache_size;
  int last_conflicting_BB;
};


OTAWA_RUN(MyApp);
