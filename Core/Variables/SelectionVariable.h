#ifndef SELECTION_VARIABLE_H_
#define SELECTION_VARIABLE_H_

/*
 *  SelectionVariable.h
 *  MWorksCore
 *
 *  Created by David Cox on 3/1/06.
 *  Copyright 2006 __MyCompanyName__. All rights reserved.
 *
 */

#include "Selection.h"
#include "Selectable.h"
#include "GenericVariable.h"
#include "ComponentFactory.h"
namespace mw {

class SelectionVariable :  public Selectable, public Variable {

protected:

	vector< shared_ptr<Variable> > values;
	shared_ptr<Variable> selected_value;
public:

	SelectionVariable(VariableProperties *props);
	SelectionVariable(VariableProperties *props, shared_ptr<Selection> _sel);
	
		
	//mSelectionVariable(const SelectionVariable& tocopy);
		
				
	virtual ~SelectionVariable(){ }
	

	virtual void addValue(shared_ptr<Variable> _var){
		values.push_back(_var);
		if(selection != NULL){
			selection->reset();
		}
	}
	
	virtual shared_ptr<Variable> getValue(int i){
		return values[i];
	}
	
	
	// A polymorphic copy constructor
	virtual Variable *clone();
	
	virtual void nextValue();
	
	virtual Datum getValue();
	virtual void setValue(Datum data){  return; }
	virtual void setValue(Datum data, MWTime time){ return; }
	virtual void setSilentValue(Datum data){  return; }
	
	virtual int getNChildren(){ return values.size();  }
	
			
	// From Selectable
	virtual int getNItems(){ return getNChildren(); }
	virtual void resetSelections() {
		selected_value = shared_ptr<Variable>();
		selection->reset();
	}
	
	
	void rejectSelections() {
		selection->rejectSelections();
		this->nextValue();
	}

};


class SelectionVariableFactory : public ComponentFactory {
	virtual shared_ptr<mw::Component> createObject(std::map<std::string, std::string> parameters,
												ComponentRegistry *reg);
};
}
#endif
