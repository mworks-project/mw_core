/*
 *  XMLParser.cpp
 *  MWorksCore
 *
 *  Created by David Cox on 11/25/07.
 *  Copyright 2007 __MyCompanyName__. All rights reserved.
 *
 */

#include "XMLParser.h"
#include "States.h"

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <glob.h>

#include <boost/tokenizer.hpp>
#include <boost/regex.hpp>
#include <boost/scope_exit.hpp>

using namespace mw;

namespace mw {
	void	error_func(void * _parser_context, const char * error, ...){
		
		va_list ap;
		va_start(ap, error);
		
		//mXMLParser *parser = (XMLParser *)_parser;
		//parser->addParserError(error);
#define ERROR_MSG_BUFFER_SIZE	512
		char buffer[ERROR_MSG_BUFFER_SIZE];// = { '\0' };
		vsnprintf(buffer, ERROR_MSG_BUFFER_SIZE-1, error, ap);
		va_end(ap);
		
		xmlParserCtxt *context = (xmlParserCtxt *)_parser_context;
		XMLParser *parser = (XMLParser *)context->_private;
        if(parser != NULL){
            string error_string((char *)buffer);
            parser->addParserError(error_string);
        }
		cerr << buffer << endl;
	}
}


void XMLParser::setup(shared_ptr<ComponentRegistry> _reg, std::string _path, std::string _simplification_transform_path){
	path = _path;
	registry = _reg;
	
	LIBXML_TEST_VERSION
	
	context = xmlNewParserCtxt();	
	if(context == NULL){
		throw SimpleException("Failed to create XML Parser Context");
	}
	
	context->_private = (void *)this;
	xmlSetGenericErrorFunc(context, &error_func);
	
	// parse the XSLT simplification transform
	simplification_transform = xsltParseStylesheetFile((const xmlChar *)(_simplification_transform_path.c_str()));
	
	// parse the file and get the DOM 
	xml_doc = xmlCtxtReadFile(context, path.c_str(), NULL, 0);
	
	errors_doc = NULL;
}

XMLParser::XMLParser(shared_ptr<ComponentRegistry> _reg, std::string _path, std::string _simplification_transform_path){
  std::string simplification_path;
  
  if(_simplification_transform_path.empty()){
    simplification_path = (prependResourcePath(std::string("MWParserTransformation.xsl"))).string();
  } else {
    simplification_path = (prependResourcePath(std::string(_simplification_transform_path))).string();
  }
  
	setup(_reg, _path, simplification_path);
}

XMLParser::XMLParser(std::string _path, std::string _simplification_transform_path){
	shared_ptr<ComponentRegistry> dummy(new ComponentRegistry());

  std::string simplification_path;
  
  if(_simplification_transform_path.empty()){
    simplification_path = (prependResourcePath(std::string("MWParserTransformation.xsl"))).string();
  } else {
    simplification_path = (prependResourcePath(std::string(_simplification_transform_path))).string();
  }
  
	setup(dummy, _path, simplification_path);
}




XMLParser::~XMLParser() {
	if(xml_doc != NULL){
		xmlFreeDoc(xml_doc);
	}
	
	if(errors_doc != NULL){
		xmlFreeDoc(errors_doc);
	}
	
	if(context != NULL){
		xmlFreeParserCtxt(context);
	}
}


void XMLParser::validate(){ }


void XMLParser::parse(bool announce_progress){ 
	
	if(xml_doc == NULL){
		string complete_message = "The following parser errors were encountered: \n\n";
		
		vector<string>::iterator i;
		for(i = parser_errors.begin(); i != parser_errors.end(); i++){
			complete_message += *i;
		}
		throw MalformedXMLException(complete_message);
	}
	
	
	// TODO: destroy exisiting resources
	
	xmlDoc *simplified = xsltApplyStylesheet(simplification_transform, xml_doc, NULL);
	
	//xmlDocDump(stderr, simplified);
	xmlNode *root_element = xmlDocGetRootElement(simplified);
	
	// TODO: check the root node to make sure it is okay
	// TODO: pull out the "passthrough" node and send it out as an event
	//		 any errors will be reported with reference id's that are contained
	//		 in this passthrough document
	
	xmlNode *child = root_element->children;
	
	// quickly count the number of nodes
	int n_nodes = 0;
	xmlNode *_child = child;
	while(_child != NULL){
		_child = _child->next;
		n_nodes++;
	}
	
	int counter = 0;
	while(child != NULL){
		StandardVariables::experimentLoadProgress->setValue((double)counter / (double)n_nodes);
		//mprintf("progress: %g", (double)(*experimentLoadProgress));
		_processNode(child);
		counter++;
		child = child->next;
	}
  
	
}


string XMLParser::squashFileName(string name){
	boost::algorithm::replace_all(name, "/", "_");
	boost::algorithm::replace_all(name, "~", "_");
	boost::algorithm::replace_all(name, " ", "__");
	
	return name;
}


void XMLParser::_processNode(xmlNode *child){
	string name((const char *)(child->name));
	
    
    try {
        if(name == "mw_create"){
            _processCreateDirective(child);
        } else if(name == "mw_anonymous_create"){
            _processAnonymousCreateDirective(child);
        } else if(name == "mw_instance"){
            _processInstanceDirective(child);
        } else if(name == "mw_connect"){
            _processConnectDirective(child);
        } else if(name == "mw_range_replicator"){
            _processRangeReplicator(child);
        } else if(name == "mw_list_replicator"){
            _processListReplicator(child);
        } else if(name == "mw_finalize"){
            _processFinalizeDirective(child);
        } else if(name == "variable_assignment"){
            _processVariableAssignment(child);
        } else if(name == "mw_passthrough"){
            // do nothing
        } else if(name == "text"){
            // do nothing
        }
    } catch (SimpleException e){
        
        mdebug("Error in %s node", name.c_str());
        xmlBufferPtr xml_buffer = xmlBufferCreate();
        int bytes = xmlNodeDump(xml_buffer, xml_doc, child, 1, 1);
        if(bytes != -1){
            string content((const char *)xml_buffer->content);
            mdebug(content.c_str());
        }
        
        throw e; // pass the buck
    }
}


//void XMLParser::_processRangeReplicator(xmlNode *node){
//	
//	string variable(_attributeForName(node, "variable"));
//	string from_string(_attributeForName(node, "from"));
//	string to_string(_attributeForName(node, "to"));
//	string step_string(_attributeForName(node, "step"));
//	
//	double from = boost::lexical_cast<double>(from_string);
//	double to = boost::lexical_cast<double>(to_string);
//	double step = boost::lexical_cast<double>(step_string);
//	
//	int instance_number=0, i = 0;
//	for(double v = from; v <= to; v += step){
//		
//		instance_number = i++;
//		string current_level_instance_id((boost::format("%d")%instance_number).str());
//		
//		char v_char_buffer[1024];
//		sprintf(v_char_buffer, "%g", v);
//		string v_string(v_char_buffer);
//		
//		xmlNode *child = node->children;
//		while(child){
//			string name((const char *)(child->name));
//			if(xmlNodeIsText(child)){
//				child = child->next;
//				continue;
//			}
//			
//			xmlNode *child_copy = xmlCopyNode(child, 1);
//			_substituteAttributeStrings(child_copy, variable, v_string);
//			
//			string reference_id = _attributeForName(child_copy, "reference_id");
//			
//			string instance_id;
//			string node_instance_id(_attributeForName(node, "instance_id"));
//			if(node_instance_id.empty()){
//				instance_id = current_level_instance_id;
//			} else {
//				instance_id = node_instance_id + string(INSTANCE_STEM) + current_level_instance_id;
//			}
//			_setAttributeForName(child_copy, "instance_id", instance_id); 
//			
//			
//			if(name == "mw_instance"){
//				// add variable assignments
//				xmlNode *assignment_node = xmlNewChild(child_copy, NULL, (const xmlChar*)"variable_assignment", (const xmlChar*)v_string.c_str());
//				_setAttributeForName(assignment_node, "variable", variable.c_str());
//				_processNode(child_copy);
//			} else {
//				_processNode(child_copy);
//			}
//			
//			xmlFreeNode(child_copy);
//			child = child->next;
//		}
//		
//	}
//	
//}

void XMLParser::_processRangeReplicator(xmlNode *node){
	string variable(_attributeForName(node, "variable"));
	vector<string> values;
    _generateRangeReplicatorValues(node, values);
	_createAndAddReplicatedChildren(node, variable, values);
}	

void XMLParser::_generateRangeReplicatorValues(xmlNode *node, vector<string> &values) {
    const int numParams = 3;
    
    vector<string> paramStrings(numParams);
	paramStrings[0] = _attributeForName(node, "from");
	paramStrings[1] = _attributeForName(node, "to");
	paramStrings[2] = _attributeForName(node, "step");
    
    vector<double> params(numParams);
    for (int i = 0; i < numParams; i++) {
        try {
            params[i] = boost::lexical_cast<double>(paramStrings[i]);
        } catch (bad_lexical_cast &) {
            throw InvalidXMLException(_attributeForName(node, "reference_id"),
                                      "Non-numeric parameter in range replicator",
                                      paramStrings[i]);
        }
    }
	
	for (double v = params[0]; v <= params[1]; v += params[2]) {
		values.push_back(boost::lexical_cast<string>(v));
	}
}

void XMLParser::_processListReplicator(xmlNode *node){
	string variable(_attributeForName(node, "variable"));
    std::vector<std::string> values;
    _generateListReplicatorValues(node, values);
	_createAndAddReplicatedChildren(node, variable, values);
}

void XMLParser::_generateListReplicatorValues(xmlNode *node, vector<string> &values) {
    static const boost::regex filenamesRegex("^FILENAMES\\((?<pattern>.+)\\)$", boost::regex::icase);

	string values_string(_attributeForName(node, "values"));
	
    typedef boost::tokenizer< boost::escaped_list_separator<char> > tokenizer;
    tokenizer tok(values_string);

    for (tokenizer::iterator iter = tok.begin(); iter != tok.end(); iter++) {
        string value(*iter);
        boost::algorithm::trim(value);

        boost::smatch matchResult;
        if (!boost::regex_match(value, matchResult, filenamesRegex)) {
            values.push_back(value);
        } else {
            _generateListReplicatorFilenames(node, values, matchResult.str("pattern"));
        }
    }
}

void XMLParser::_generateListReplicatorFilenames(xmlNode *node, vector<string> &values, const string &pattern) {
    int cwdfd = open(".", O_RDONLY);
    if (-1 == cwdfd) {
        throw SimpleException("Unable to open current working directory", strerror(errno));
    }
    BOOST_SCOPE_EXIT( (&cwdfd) )
    {
        (void)fchdir(cwdfd);
        (void)close(cwdfd);
    } BOOST_SCOPE_EXIT_END
 
    string workingPath(getWorkingPathString());
    if (!(workingPath.empty())) {
        if (-1 == chdir(workingPath.c_str())) {
            throw SimpleException("Unable to change directory", strerror(errno));
        }
    }
    
    glob_t globResults;
    int globStatus = glob(pattern.c_str(), 0, NULL, &globResults);
    BOOST_SCOPE_EXIT( (&globResults) )
    {
        globfree(&globResults);
    } BOOST_SCOPE_EXIT_END
    
    if ((0 != globStatus) && (GLOB_NOMATCH != globStatus)) {
        throw InvalidXMLException(_attributeForName(node, "reference_id"),
                                  "Unknown error when processing list replicator FILENAMES() expression",
                                  pattern);
    }
    
    namespace bf = boost::filesystem;
    int numMatches = 0;

    for (int i = 0; i < globResults.gl_pathc; i++) {
        bf::path filePath(globResults.gl_pathv[i], bf::native);
        if (bf::is_regular_file(filePath)) {
            values.push_back(filePath.string());
            numMatches++;
        }
    }
    
    if (0 == numMatches) {
        throw InvalidXMLException(_attributeForName(node, "reference_id"),
                                  "No matches for list replicator FILENAMES() expression",
                                  pattern);
    }
}

void XMLParser::_createAndAddReplicatedChildren(xmlNode *node, 
												const string &variable, 
												const vector<string> &values) {
	int instance_number=0;
	int i = 0;
	for(vector<string>::const_iterator value = values.begin();
		value != values.end();
		++value) {
		
		instance_number = i++;
		string current_level_instance_id((boost::format("%d")%instance_number).str());
		
		xmlNode *child = node->children;
		while(child){
			string name((const char *)(child->name));
			if(xmlNodeIsText(child)){
				child = child->next;
				continue;
			}
			
			xmlNode *child_copy = xmlCopyNode(child, 1);
			_substituteAttributeStrings(child_copy, variable, *value);
			_substituteTagStrings(child_copy, variable, *value);
            
			string reference_id = _attributeForName(child_copy, "reference_id");
			
			string instance_id;
			string node_instance_id(_attributeForName(node, "instance_id"));
			if(node_instance_id.empty()){
				instance_id = current_level_instance_id;
			} else {
				instance_id = node_instance_id + string(INSTANCE_STEM) + current_level_instance_id;
			}
			_setAttributeForName(child_copy, "instance_id", instance_id); 
			
			
			if(name == "mw_instance" || name == "mw_range_replicator" || name == "mw_list_replicator"){  // or range rep?
				// add variable assignments
				_addVariableAssignment(child_copy, variable, *value); 
				_processNode(child_copy);
			} else {
				_processNode(child_copy);
			}
			
			xmlFreeNode(child_copy);
			child = child->next;
		}		
	}
}

void XMLParser::_addVariableAssignment(xmlNode *node, const string& variable, const string& value){
	
	string name((const char *)(node->name));
	if(name == "mw_range_replicator" || name == "mw_list_replicator"){
		xmlNode *child = node->children;
		while(child != NULL){
			_addVariableAssignment(child, variable, value);
			child = child->next;
		}
	} else {
		xmlNode *assignment_node = xmlNewChild(node, NULL, (const xmlChar*)"variable_assignment", (const xmlChar*)value.c_str());
		_setAttributeForName(assignment_node, "variable", variable.c_str());
	}
}

void XMLParser::_dumpNode(xmlNode *node){
	
	_xmlAttr *att = node->properties;
	
	while( att != NULL){
		cerr << "\t" << att->name << ":" 
		<< att->children->content << endl;
		
		att = att->next;
	}
}

void XMLParser::_substituteAttributeStrings(xmlNode *node, string token, string replacement){
	static string prefix1("$");
	static string prefix2("${");
  static string suffix2("}");
  
  shared_ptr<string> form1_ptr(new string(prefix1 + token));
  shared_ptr<string> form2_ptr(new string(prefix2 + token + suffix2));
  shared_ptr<string> replacement_ptr(new string(replacement));
  
  _substituteAttributeStrings(node, form1_ptr, form2_ptr, replacement_ptr);
}
  	
                          
void XMLParser::_substituteAttributeStrings(xmlNode *node, shared_ptr<string> form1, shared_ptr<string> form2, shared_ptr<string> replacement){
  
  
	if(xmlNodeIsText(node)){
		string content((const char *)xmlNodeGetContent(node));
		boost::replace_all(content, *form1, *replacement);
		boost::replace_all(content, *form2, *replacement);
		xmlNodeSetContent(node, (const xmlChar *)(content.c_str()));
		return;
	} else {
		
		xmlNode *child = node->children;
		
		while( child != NULL ){
			
			_substituteAttributeStrings(child, form1, form2, replacement);
			child = child->next;
		}
	}
	
}

void XMLParser::_substituteTagStrings(xmlNode *node, string token, string replacement){
	static string prefix1("$");
	static string prefix2("${");
  static string suffix2("}");
  
  shared_ptr<string> form1_ptr(new string(prefix1 + token));
  shared_ptr<string> form2_ptr(new string(prefix2 + token + suffix2));
  shared_ptr<string> replacement_ptr(new string(replacement));
  
  _substituteTagStrings(node, form1_ptr, form2_ptr, replacement_ptr);
}

void XMLParser::_substituteTagStrings(xmlNode *node, shared_ptr<string> form1, shared_ptr<string> form2, shared_ptr<string> replacement){
    
    if(xmlNodeIsText(node)){
        return;
    }
    
    string content = _attributeForName(node, "tag");
    if(!content.empty()){
        boost::replace_all(content, *form1, *replacement);
        boost::replace_all(content, *form2, *replacement);
        _setAttributeForName(node, "tag", content);
    }
    
    xmlNode *child = node->children;
    
    while( child != NULL ){
        _substituteTagStrings(child, form1, form2, replacement);
        child = child->next;
    }
}

std::string XMLParser::getWorkingPathString(){
	
	try{
		std::string escaped_path_string = path;
		//boost::replace_all(escaped_path_string, " ", "\\ ");
		boost::filesystem::path the_path(escaped_path_string, boost::filesystem::native);
		boost::filesystem::path the_branch = the_path.branch_path();
		std::string branch_string = the_branch.string();
		
		return branch_string;
		
	} catch (std::exception& e) {
		
		cerr << "Path no good:" << path << ":" << e.what() << endl;
	}
	
	return "";
}

void XMLParser::_processFinalizeDirective(xmlNode *node){
	
	string tag(_attributeForName(node, "object"));
	string reference_id(_attributeForName(node, "reference_id"));
	string instance_id(_attributeForName(node, "instance_id"));
	
	if(instance_id.empty()){
		instance_id = "0";
	}
	
	map<string, string> properties = _createPropertiesMap(node);
	
	shared_ptr<mw::Component> object; 
	
	// Try to lookup by instance
	string instance_tag = _generateInstanceTag(tag, reference_id, instance_id);
	object = registry->getObject<mw::Component>(instance_tag);
	string used_tag = instance_tag;
	
	// Try to lookup by tag
	if(object == NULL){
		object = registry->getObject<mw::Component>(tag);
		used_tag = tag;
	}
	
	// If that fails, try by reference id
	if(object == NULL){
		object = registry->getObject<mw::Component>(reference_id);
		used_tag = reference_id;
	}
	
	
	if(object == NULL && properties.find("alt") != properties.end()){
		object = registry->getObject<mw::Component>(properties["alt"]);
        if(object != NULL){
			registry->registerObject(tag, object, true);
		}
	}
	
	if(object != NULL){
		object->finalize(properties, registry.get());
		//mprintf(M_PARSER_MESSAGE_DOMAIN,
		//				"Finalized object \"%s\"", used_tag.c_str());
	} else {
		// TODO: throw
		throw InvalidXMLException(reference_id, 
								  "Failed to finalize object", tag);
	}
	
	
}


void XMLParser::_processAnonymousCreateDirective(xmlNode *node){
	_processGenericCreateDirective(node,1);
}

void XMLParser::_processCreateDirective(xmlNode *node){
	_processGenericCreateDirective(node, 0);
}

void XMLParser::_processGenericCreateDirective(xmlNode *node, bool anon){
    string object_type(_attributeForName(node, "object"));
    string reference_id(_attributeForName(node, "reference_id"));
    string parent_scope(_attributeForName(node, "parent_scope"));
	
	// Create an STL map containing all properties contained under this
	// node
	map<string, string> properties = _createPropertiesMap(node);
	
    string tag;
    if(parent_scope.empty()){
        tag = properties["tag"];
    } else {
        tag = parent_scope + "/" + properties["tag"];
        properties["parent_scope"] = parent_scope;
    }
	
	// Add XML document information to this properties map
	// (for instance, if you need to know the working path...)
	properties["xml_document_path"] = path;
	
	std::string branch_string = getWorkingPathString();
	properties["working_path"] = branch_string.c_str();
	
	
	// Concatenate a type identifier to the object type string
	// e.g. if type = "itc18", then object_type would be iodevice/itc18
	if(!(properties["type"].empty())){
		object_type += "/" + boost::to_lower_copy(properties["type"]);
	}
	
	
	shared_ptr<mw::Component> component;
	
	try {
		// Create a new object and register it using it's reference id.
		// this id is generated by the xslt transformation, and is
		// guaranteed to be unique within the XML document
		component = registry->registerNewObject(reference_id, object_type, properties);
		
		// Also register the object using it's tag.  These are usually unique,
		// but are sometimes missing or duplicated (e.g. task system states can
		// have the same names in different task systems, while remaining valid
		// If the create directive is "anonymous," then ignore the tag.  See 
		// comments for _procressAnonymousCreateDirective.
		if(!anon && !tag.empty()){
			registry->registerObject(tag, component);
		}
		
	} catch(SimpleException& e){
		
        
		// Objects are allowed to indicate an alternative object to use if 
		// the create directive fails (this is common with IO Devices, which 
		// will fail to create if the device is not currently connected.
		// Alt substitution is the responsibility of the object class in question,
		// and is usually handled in a "finalize" directive
        bool allow_failover = (bool)(StandardVariables::alt_failover->getValue());
		if(properties.find("alt") != properties.end() && allow_failover){
			mwarning(M_PARSER_MESSAGE_DOMAIN,
					 "Failed to create object \"%s\" of type %s (but alt object is specified)",
					 tag.c_str(), object_type.c_str());
		} else {
        
            if(properties.find("alt") != properties.end()){
                merror(M_PARSER_MESSAGE_DOMAIN, 
                       "An 'alt' object is specified, but the #allowAltFailover setup variable is set to disallow failover");
            }
            
            //merror(M_PARSER_MESSAGE_DOMAIN, 
			//	   "Failed to create object \"%s\" of type %s (error was:\"%s\")", tag.c_str(), object_type.c_str(), e.what());
        
            
            throw InvalidXMLException(reference_id, (const SimpleException&)e);
		}
	}
	
	if(component == NULL){
		mwarning(M_PARSER_MESSAGE_DOMAIN, 
				 "Failed to create object \"%s\" of type %s", tag.c_str(), object_type.c_str());
		// TODO: throw
	} else {
		component->setReferenceID(reference_id);
		//mprintf(M_PARSER_MESSAGE_DOMAIN,
		//				"Created object \"%s\" of type %s", tag.c_str(), object_type.c_str());
	}
}

void XMLParser::_processInstanceDirective(xmlNode *node){
	string tag(_cStringAttributeForName(node, "object"));
	string reference_id(_cStringAttributeForName(node, "reference_id"));
	
	// "Instances" have an id associated with them. When a replicator
	string instance_id(_cStringAttributeForName(node, "instance_id"));
	if(instance_id.empty()){
		instance_id = "0";
	}
	
	// Try to look up the object by the provided tag
	shared_ptr<mw::Component> object = registry->getObject<mw::Component>(tag);
	
	// If that fails, look it up by its reference_id
	if(object == NULL){
		// "Anonymous" style name
		object = registry->getObject<mw::Component>(reference_id);
	}
	
	// If the object is still unfound, something is very wrong
	if(object == NULL){
		throw  InvalidXMLException(reference_id,
								   "Unknown object during aliasing phase",tag);
	}
	
	// Create an "instance" of the object.  What this means will vary from
	// object to object.  In the case of paradigm components (e.g. block, trial)
	// this produces a shallow copy with it's own separate variable context and
	// selection machinery, but with children, etc. shared with the original
	shared_ptr<mw::Component> alias = object->createInstanceObject();
	string instance_tag = _generateInstanceTag(tag, reference_id, instance_id);
	
	// If the object is a valid variable environment, we'll apply variable
	// assignments if there are any
	// Look for variable assignments if appropriate
	shared_ptr<ScopedVariableEnvironment> env = dynamic_pointer_cast<ScopedVariableEnvironment, mw::Component>(alias);
	if(env != NULL){
		
		xmlNode *alias_child = node->children;
		
		while(alias_child != NULL){
			string child_name((const char *)alias_child->name);
			
			if(child_name == "variable_assignment"){
				string variable_name = _attributeForName(alias_child, "variable");
				shared_ptr<Variable> var = registry->getVariable(variable_name);
				
				if(var == NULL){
					// TODO: better throw
					throw InvalidXMLException(reference_id,
											  "Invalid variable", variable_name);
				}

				string content((const char *)xmlNodeGetContent(alias_child));
                GenericDataType dataType = var->getProperties()->getDefaultValue().getDataType();
                Datum value = registry->getNumber(content, dataType);
				
				if(value.isUndefined()){
					// TODO: better throw
					throw InvalidXMLException(reference_id,
											  "Invalid value", content);
				} 
				
				shared_ptr<ScopedVariable> svar = dynamic_pointer_cast<ScopedVariable, Variable>(var);
				
				if(svar == NULL){
					// TODO: better throw
					throw InvalidXMLException(reference_id,
											  "Cannot assign a non-local variable", variable_name);
				} 
				
				env->setValue(svar->getContextIndex(), value);
				//mprintf(M_PARSER_MESSAGE_DOMAIN, "Assigned variable %s to value %s in the context of %s",
				//						variable_name.c_str(), content.c_str(), instance_tag.c_str());
				
			}
			
			alias_child = alias_child->next;
		}
		
	}
	
	registry->registerObject(instance_tag, alias);
	
	// Check to be sure that the object was registered properly
	shared_ptr<mw::Component> test = registry->getObject<mw::Component>(instance_tag);
	
	if(alias.get() != test.get()){
		throw  InvalidXMLException(reference_id,
								   "Failed to register component", instance_tag);
		return;
	}
	
#define VERBOSE_PARSER	1
#ifdef VERBOSE_PARSER
	//mprintf(M_PARSER_MESSAGE_DOMAIN,
	//			"Created an alias (%s) of object %s", instance_tag.c_str(), tag.c_str());
#endif
}


shared_ptr<mw::Component> XMLParser::_getConnectionChild(xmlNode *child){
	
	//string child_name((const char *)child->name);
	string child_tag(_attributeForName(child, "tag"));
	string child_reference_id(_attributeForName(child, "reference_id"));
	string child_instance_id(_attributeForName(child, "instance_id"));
	if(child_instance_id.empty()){
		child_instance_id = "0";
	}
	
	// First look up if there is an "instanced" version of this object
	// as these versions always have priority if they exist
	string child_instance_tag = _generateInstanceTag(child_tag, child_reference_id, child_instance_id);
	
	shared_ptr<mw::Component> child_component;
    
    try {
        child_component = registry->getObject<mw::Component>(child_instance_tag);
	} catch (AmbiguousComponentReferenceException e){
        // no problem, we're going to check by tag name next
    }
    
	// if it exists, return it
	if(child_component != NULL){
		return child_component;
	}
	

    try {
	
        child_component = registry->getObject<mw::Component>(child_tag);
        

        if(child_component != NULL){
            return child_component;
        }
	
	
        // Finally, try to look up the object using its reference_id
        child_component = registry->getObject<mw::Component>(child_reference_id);
	} catch (AmbiguousComponentReferenceException e){
        merror(M_PARSER_MESSAGE_DOMAIN, "An ambiguously named object was detected during parsing (connection phase).\n"
                                        "Please ensure that all object names are unique.\n"
                                        "Details: tag_name = <%s>, reference_id = <%s>, instance_id = <%s>", 
                            child_tag.c_str(), child_reference_id.c_str(), child_instance_id.c_str());
        registry->dumpToStdErr();
        throw FatalParserException();
    }
	
	if(child_component == NULL){
		// do nothing
	}
	
	return child_component;
}


void XMLParser::_connectChildToParent(shared_ptr<mw::Component> parent, 
									  map<string, string> properties, 
									  xmlNode *child_node){
	
	string child_name((const char *)child_node->name);
	string child_reference_id(_attributeForName(child_node, "reference_id"));
	string child_instance_id(_attributeForName(child_node, "instance_id"));
	string child_tag(_attributeForName(child_node, "tag"));
	string parent_tag = properties["tag"];
	
	// if the node is a replicator, we'll recursively call this method
	// on each of the replicator's children
	if(child_name == "mw_range_replicator" || child_name == "mw_list_replicator"){
		string variable(_attributeForName(child_node, "variable"));
        vector<string> values;

		if(child_name == "mw_range_replicator") {
            _generateRangeReplicatorValues(child_node, values);
		} else { // "mw_list_replicator"
            _generateListReplicatorValues(child_node, values);
		}

        _createAndConnectReplicatedChildren(parent, 
                                            properties,
                                            child_node,
                                            child_instance_id,
                                            variable,
                                            values);
		// Otherwise, just connect the child to parent
	} else {
		
		shared_ptr<mw::Component> child_component = _getConnectionChild(child_node);
		
		if(child_component != NULL){
			parent->addChild(properties, registry.get(), child_component);
			//mprintf(M_PARSER_MESSAGE_DOMAIN,
			//			"Connected child (%s / %s) to parent %s", child_tag.c_str(), 
			//										child_reference_id.c_str(),
			//										parent_tag.c_str());
		} else {
			string message((boost::format("Could not find child (%s) to connect to parent (%s)") % child_tag % parent_tag).str());
			//throw InvalidXMLException(child_reference_id, message);
			
			// is this warning necessary?
			// mwarning(M_PARSER_MESSAGE_DOMAIN, message.c_str());
		}
	}
}


void XMLParser::_createAndConnectReplicatedChildren(shared_ptr<mw::Component> parent, 
													map<string, string> properties,
													xmlNode *child_node,
													const string &child_instance_id,
													const string &variable, 
													const vector<string> &values) {
	int i = 0;
	for(vector<string>::const_iterator value = values.begin();
		value != values.end();
		++value) {
		
		int instance_number = i++;
		string current_level_instance_id((boost::format("%d") % instance_number).str());
		
		string subchild_instance_id;
		if(child_instance_id.empty()){
			subchild_instance_id = current_level_instance_id;
		} else {
			subchild_instance_id = child_instance_id;
			subchild_instance_id += string(INSTANCE_STEM) + current_level_instance_id;
		}
		
		xmlNode *subchild = child_node->children;
		
		while(subchild != NULL){
			
			xmlNode *subchild_copy = xmlCopyNode(subchild,1);
			
            _substituteTagStrings(subchild_copy, variable, *value);
            
			// perform substitution in the variable's tag, if necessary
//			string prefix("$");
//			string form1 = prefix + variable;
//			string form2 = prefix + string("{") + variable + string("}");
//			string content = _attributeForName(subchild, "tag");
//			boost::replace_all(content, form1, *value);
//			boost::replace_all(content, form2, *value);
//			_setAttributeForName(subchild_copy, "tag", content);
			
			_setAttributeForName(subchild_copy, "instance_id", subchild_instance_id);
			
			_connectChildToParent(parent, properties, subchild_copy);
			
			xmlFreeNode(subchild_copy);
			subchild = subchild->next;
		}
	}
}


void XMLParser::_processConnectDirective(xmlNode *node){
	
	string parent_tag(_attributeForName(node, "parent"));
	string reference_id(_attributeForName(node, "reference_id"));
    string parent_scope(_attributeForName(node, "parent_scope"));
    
	map<string, string> properties;
	
    if(!parent_scope.empty()){
        parent_tag = parent_scope + "/" + parent_tag;
    }
    
    properties["parent_tag"] = parent_tag;
	properties["parent_reference_id"] = reference_id;
	
	xmlNode *child = node->children;
	
	shared_ptr<mw::Component> parent_component;
	
	// Try to look up object with its reference id	
    try {
        parent_component = registry->getObject<mw::Component>(reference_id);
    } catch (AmbiguousComponentReferenceException e){
        // not necessarily a big deal, move on to a more descriptive name
    }
    
	// If that didn't work, try using the tag
	if(parent_component == NULL){
		parent_component = registry->getObject<mw::Component>(parent_tag);
	}
	
	if(parent_component == NULL){
		// error (should eventually throw)
        // For now, this doesn't interact well with alt objects (e.g.
        // iodevices, when the device itself is not present)
		// throw SimpleException("Unknown object during connection phase",
		//														parent_tag);
		return;
	}
	
	while(child != NULL){
		
		_connectChildToParent(parent_component, properties, child);		
		child = child->next;
	}
	
}


string XMLParser::_attributeForName(xmlNode *node, string name){
  const char *result = _cStringAttributeForName(node, name);
  if(result == NULL){
    return string();
  } else {
    return string(result);
  }
}

const char * XMLParser::_cStringAttributeForName(xmlNode *node, string name){

  // Parse the attributes
	_xmlAttr *att = node->properties;
	
	while( att != NULL){
		
		if(name == (const char *)(att->name)){
			return (const char *)(att->children->content);
		}			
		att = att->next;
	}
	
	return NULL;
}

void XMLParser::_setAttributeForName(xmlNode *node, string name, string value){
	
	//	_xmlAttr *att = node->properties;
	
	xmlSetProp(node, (const xmlChar *)name.c_str(), (const xmlChar *)value.c_str());
}



map<string, string> XMLParser::_createPropertiesMap(xmlNode *node){
	
	map<string, string> returnmap;
	
	xmlNode *child = node->children;
	
	while(child != NULL){
		string name((const char *)child->name);
		if(!xmlNodeIsText(child)){
      if(child->children != NULL && child->children->content != NULL){
        returnmap[name] = (const char *)(child->children->content);
      }
		}
		child = child->next;
	}
	
	return returnmap;
}


vector<xmlNode *> XMLParser::_getChildren(xmlNode *node){
	string empty("");
	return _getChildren(node, empty);
}

vector<xmlNode *> XMLParser::_getChildren(xmlNode *node, string tag){
	
	vector<xmlNode *> children;
	
	xmlNode *child = node->children;
	
	while(child != NULL){
		// TODO: check to only take nodes, not text and stuff
		
		if( !xmlNodeIsText(child) &&
		   (tag.empty() || tag == (const char *)child->name)){
			children.push_back(child);
		}
		child = child->next;
	}
	
	return children;
}

string XMLParser::_getContent(xmlNode *node){
	
	string returnval((const char *)xmlNodeGetContent(node));
	return returnval;
}


Datum XMLParser::_parseDataValue(xmlNode *node){
	
	// if node has "value" field, just use that
	// call getValue() to convert the string into a datum
	string value_field_contents = _attributeForName(node, "value");
	string type_string = _attributeForName(node, "type");
	string reference_id = _attributeForName(node, "reference_id");
	if(reference_id.empty()){
		reference_id = "unknown";
	}
	
	GenericDataType type = M_UNDEFINED;
	
	// TODO standardize this
	if(type_string == "integer"){
		type = M_INTEGER;
	} else if(type_string == "float"){
		type = M_FLOAT;
	} else if(type_string == "boolean" || type_string == "bool"){
		type = M_BOOLEAN;
	} else if (type_string == "string"){
		type = M_STRING;
	}
	
	if(value_field_contents.empty() == false){
		
		if(type == M_STRING){
			return Datum(value_field_contents);
		} else if(type != M_UNDEFINED){
			return registry->getNumber(value_field_contents, type);
		} else {
			return Datum(value_field_contents);
		}
	}
	
	// otherwise, look into the contents of the node
	vector<xmlNode *> children = _getChildren(node);
	
	if(children.size() == 0){
		if(type != M_UNDEFINED && type != M_STRING){
			return registry->getNumber(_getContent(node), type);
		} else {
			return Datum(_getContent(node));
		}
	}
	
	if(children.size() > 1){
		// TODO: better throw
		throw InvalidXMLException(reference_id, "Invalid variable assignment");
	}
	
	xmlNode *child = children[0];
	
	string child_name((const char *)child->name);
	
	// if there is a <dictionary> node inside
	if(child_name == "dictionary"){
		
		vector<xmlNode *> elements = _getChildren(child, "dictionary_element");
		
		
		vector<Datum> keys;
		vector<Datum> values;
		
		for(vector<xmlNode *>::iterator el = elements.begin();
			el != elements.end(); 
			++el) {
			vector<xmlNode *> key_nodes = _getChildren(*el, "key");
			if(key_nodes.size() != 1){
				// TODO: better throw
				throw InvalidXMLException(reference_id,
										  "Cannot have more than one key in a dictionary element");
			}
			
			xmlNode *key_node = key_nodes[0];
			
			vector<xmlNode *> value_nodes = _getChildren(*el, "value");
			if(value_nodes.size() != 1){
				throw InvalidXMLException(reference_id,
										  "Cannot have more than one value in a dictionary element");
			}
			
			xmlNode *value_node = value_nodes[0];
            Datum value_data = _parseDataValue(value_node);
			values.push_back(value_data);
            Datum key_data = _parseDataValue(key_node);
			keys.push_back(key_data);
		}
		
        Datum dictionary(M_DICTIONARY, (int)keys.size());
		
		vector<Datum>::iterator k = keys.begin();
		vector<Datum>::iterator v = values.begin();
		while(k != keys.end() && v != values.end()){
			dictionary.addElement(*k, *v);
			k++; v++;
		}
		
		return dictionary;
		
		// if there is a <list> node inside
	} else if(child_name == "list_data"){
		
		vector<xmlNode *> elements = _getChildren(child, "list_element");
		
		vector<Datum> values;
		
		
		for(vector<xmlNode *>::iterator el = elements.begin(); 
			el != elements.end(); 
			++el ){
			vector<xmlNode *> value_nodes = _getChildren(*el, "value");
			if(value_nodes.size() != 1){
				throw InvalidXMLException(reference_id,
										  "Cannot have more than one value in a list element");
			}
			
			xmlNode *value_node = value_nodes[0];
		 Datum value_data = _parseDataValue(value_node);
			values.push_back(value_data);
		}
		
	 Datum list(M_LIST, (int)values.size());
		
		vector<Datum>::iterator v = values.begin();
		while(v != values.end()){
			list.addElement(*v);
			v++;
		}
		
		return list;
		
		// if there is an <integer> node inside
	} else if(child_name == "integer"){
	 Datum value = registry->getNumber(_getContent(node));
		
	 Datum transformed(value.getInteger());
		
		return transformed;
		
	} else if(child_name == "float"){
		
	 Datum value = registry->getNumber(_getContent(node));
		
	 Datum transformed(value.getFloat());
		
		return transformed;
		
	} else {
		// TODO: better throw
		throw InvalidXMLException(reference_id, "Unknown data type", child_name);
	}
}


void XMLParser::_processVariableAssignment(xmlNode *node){
	
	string variable_name(_attributeForName(node, "variable"));
	
	shared_ptr<Variable> variable = registry->getVariable(variable_name);
	
	if(variable == NULL){
		// TODO: better throw
		if(variable_name.empty()){
			throw SimpleException("Variable missing tag attribute in variable assignment");
		} else {
			throw SimpleException("Unknown variable during variable assignment", variable_name);
		}
	}
	
    Datum value = _parseDataValue(node);
	
	variable->setValue(value);
}

