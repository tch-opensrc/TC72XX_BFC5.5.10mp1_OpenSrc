//{{{  Banner                           

//============================================================================
//
//     component.cxx
//
//     Implementation of the CdlComponent class
//
//============================================================================
//####COPYRIGHTBEGIN####
//                                                                          
// ----------------------------------------------------------------------------
// Copyright (C) 2002 Bart Veer
// Copyright (C) 1999, 2000 Red Hat, Inc.
//
// This file is part of the eCos host tools.
//
// This program is free software; you can redistribute it and/or modify it 
// under the terms of the GNU General Public License as published by the Free 
// Software Foundation; either version 2 of the License, or (at your option) 
// any later version.
// 
// This program is distributed in the hope that it will be useful, but WITHOUT 
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
// more details.
// 
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, Inc., 
// 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// ----------------------------------------------------------------------------
//                                                                          
//####COPYRIGHTEND####
//============================================================================
//#####DESCRIPTIONBEGIN####
//
// Author(s):   bartv
// Contact(s):  bartv
// Date:        1999/03/01
// Version:     0.02
//
//####DESCRIPTIONEND####
//============================================================================

//}}}
//{{{  #include's                       

// ----------------------------------------------------------------------------
#include "cdlconfig.h"

// Get the infrastructure types, assertions, tracing and similar
// facilities.
#include <cyg/infra/cyg_ass.h>
#include <cyg/infra/cyg_trac.h>

// <cdl.hxx> defines everything implemented in this module.
// It implicitly supplies <string>, <vector> and <map> because
// the class definitions rely on these headers.
#include <cdl.hxx>

//}}}

//{{{  Statics                          

// ----------------------------------------------------------------------------
CYGDBG_DEFINE_MEMLEAK_COUNTER(CdlComponentBody);

//}}}
//{{{  Constructor                      

// ----------------------------------------------------------------------------
CdlComponentBody::CdlComponentBody(std::string name_arg)
    : CdlNodeBody(name_arg),
      CdlContainerBody(),
      CdlUserVisibleBody(),
      CdlValuableBody(),
      CdlParentableBody(),
      CdlBuildableBody(),
      CdlDefinableBody()
{
    CYG_REPORT_FUNCNAME("CdlComponentBody:: constructor");
    CYG_REPORT_FUNCARG1XV(this);

    cdlcomponentbody_cookie = CdlComponentBody_Magic;
    CYGDBG_MEMLEAK_CONSTRUCTOR();

    CYG_POSTCONDITION_THISC();
    CYG_REPORT_RETURN();
}

//}}}
//{{{  Destructor                       

// ----------------------------------------------------------------------------

CdlComponentBody::~CdlComponentBody()
{
    CYG_REPORT_FUNCNAME("CdlComponentBody:: destructor");
    CYG_REPORT_FUNCARG1XV(this);
    CYG_PRECONDITION_THISC();

    cdlcomponentbody_cookie = CdlComponentBody_Invalid;
    CYGDBG_MEMLEAK_DESTRUCTOR();
    
    CYG_REPORT_RETURN();
}

//}}}
//{{{  parse_component()                

// ----------------------------------------------------------------------------
// Parsing a component definition. This routine gets invoked directly from the
// Tcl interpreter.

int
CdlComponentBody::parse_component(CdlInterpreter interp, int argc, const char* argv[])
{
    CYG_REPORT_FUNCNAMETYPE("CdlComponentBody::parse_component", "result %d");
    CYG_REPORT_FUNCARG1("argc %d", argc);
    CYG_PRECONDITION_CLASSC(interp);
    
    std::string  diag_argv0      = CdlParse::get_tcl_cmd_name(argv[0]);

    CdlLoadable  loadable       = interp->get_loadable();
    CdlPackage   package        = dynamic_cast<CdlPackage>(loadable);
    CdlContainer parent         = interp->get_container();       
    CdlToplevel  toplevel       = interp->get_toplevel();
    CYG_ASSERT_CLASSC(loadable);        // There should always be a loadable during parsing
    CYG_ASSERT_CLASSC(package);         // And packages are the only loadable for software CDL.
    CYG_ASSERT_CLASSC(parent);
    CYG_ASSERT_CLASSC(toplevel);

    // The new component should be created and added to the package
    // early on. If there is a parsing error it will get cleaned up
    // automatically as a consequence of the package destructor.
    // However it is necessary to validate the name first. Errors
    // should be reported via CdlParse::report_error(),
    // which may result in an exception.
    CdlComponent new_component  = 0;
    bool         ok             = true;
    int          result         = TCL_OK;
    try {
    
        // Currently there are no options. This may change in future.
        if (3 != argc) {
            CdlParse::report_error(interp, "",
                                   std::string("Incorrect number of arguments to `") + diag_argv0 +
                                         "'\nExpecting name and properties list.");
            ok = false;
            goto done;
        }
        if (!Tcl_CommandComplete(CDL_TCL_CONST_CAST(char*, argv[2]))) {
            CdlParse::report_error(interp, "",
                                   std::string("Invalid property list for cdl_component `") + argv[1] + "'.");
            ok = false;
            goto done;
        }

        if (0 != toplevel->lookup(argv[1])) {
            CdlParse::report_error(interp, "",
                                   std::string("Component `") + argv[1] +
                                   "' cannot be loaded.\nThe name is already in use.");
            ok = false;
        } else {
            new_component = new CdlComponentBody(argv[1]);
            toplevel->add_node(package, parent, new_component);
        }

      done:
        if (!ok) {
            // Just because this component cannot be created, that is no
            // reason to abort the whole parsing process.
            CYG_REPORT_RETVAL(TCL_OK);
            return TCL_OK;
        }
    } catch(std::bad_alloc e) {
        interp->set_result(CdlParse::construct_diagnostic(interp, "internal error", "", "Out of memory"));
        result = TCL_ERROR;
    } catch(CdlParseException e) {
        interp->set_result(e.get_message());
        result = TCL_ERROR;
    } catch(...) {
        interp->set_result(CdlParse::construct_diagnostic(interp, "internal error", "", "Unexpected C++ exception"));
        result = TCL_ERROR;
    }
    if (TCL_OK != result) {
        CYG_REPORT_RETVAL(result);
        return result;
    }

    // At this stage new_component has been created and added to the hierarchy.
    // The main work now is to add the properties.
    
    // Push the component as the current node early on. This aids
    // diagnostics. Also make it the new container.
    CdlNode      old_node       = interp->push_node(new_component);
    CdlContainer old_container  = interp->push_container(new_component);
    std::string  old_context;
    CYG_ASSERTC(parent == old_container);

    // Declare these outside the scope of the try statement, to allow
    // goto calls for the error handling.
    std::string tcl_result;
    std::vector<CdlInterpreterCommandEntry>  new_commands;
    std::vector<CdlInterpreterCommandEntry>* old_commands = 0;
    static CdlInterpreterCommandEntry commands[] =
    {
        CdlInterpreterCommandEntry("script",             &CdlComponentBody::parse_script    ),
        CdlInterpreterCommandEntry("cdl_component",      &CdlComponentBody::parse_component ),
        CdlInterpreterCommandEntry("cdl_option",         &CdlOptionBody::parse_option       ),
        CdlInterpreterCommandEntry("cdl_interface",      &CdlInterfaceBody::parse_interface ),
        CdlInterpreterCommandEntry("cdl_dialog",         &CdlDialogBody::parse_dialog       ),
        CdlInterpreterCommandEntry("cdl_wizard",         &CdlWizardBody::parse_wizard       ),
        CdlInterpreterCommandEntry("",                   0                                  )
    };
    static CdlInterpreterCommandEntry   script_commands[] =
    {
        CdlInterpreterCommandEntry("cdl_component",      &CdlComponentBody::parse_component ),
        CdlInterpreterCommandEntry("cdl_option",         &CdlOptionBody::parse_option       ),
        CdlInterpreterCommandEntry("cdl_interface",      &CdlInterfaceBody::parse_interface ),
        CdlInterpreterCommandEntry("cdl_dialog",         &CdlDialogBody::parse_dialog       ),
        CdlInterpreterCommandEntry("cdl_wizard",         &CdlWizardBody::parse_wizard       ),
        CdlInterpreterCommandEntry("",                   0                                  ),
    };
    int i;
    
    // All parsing errors may result in an exception, under the control of
    // application code. This exception must not pass through the Tcl interpreter.
    try {

        for (i = 0; 0 != commands[i].command; i++) {
            new_commands.push_back(commands[i]);
        }
        CdlBuildableBody::add_property_parsers(new_commands);
        CdlDefinableBody::add_property_parsers(new_commands);
        CdlParentableBody::add_property_parsers(new_commands);
        CdlValuableBody::add_property_parsers(new_commands);
        CdlUserVisibleBody::add_property_parsers(new_commands);
        CdlNodeBody::add_property_parsers(new_commands);
    
        // Now evaluate the body. If an error occurs then typically
        // this will be reported via CdlParse::report_error(),
        // but any exceptions will have been intercepted and
        // turned into a Tcl error.
        old_commands = interp->push_commands(new_commands);
        result = interp->eval(argv[2], tcl_result);
        interp->pop_commands(old_commands);
        
        if (TCL_OK != result) {
            // No point in taking any further action, just go with the flow
            goto done2;
        }

        // Even if there were errors, they were not fatal. There may
        // now be a number of properties for this component, and some
        // validation should take place. Start with the base classes.
        new_component->CdlNodeBody::check_properties(interp);
        new_component->CdlUserVisibleBody::check_properties(interp);
        new_component->CdlValuableBody::check_properties(interp);
        new_component->CdlParentableBody::check_properties(interp);
        new_component->CdlBuildableBody::check_properties(interp);
        new_component->CdlDefinableBody::check_properties(interp);

        // There should be at most one each of wizard and script.
        if (new_component->count_properties(CdlPropertyId_Wizard) > 1) {
            CdlParse::report_error(interp, "", "A component should have at most one `wizard' property.");
        }
        if (new_component->count_properties(CdlPropertyId_Script) > 1) {
            CdlParse::report_error(interp, "", "A component should have at most one `script' property.");
        }

        // If there is a script property, life gets more interesting.
        if (new_component->has_property(CdlPropertyId_Script)) {
            CdlProperty_String prop = dynamic_cast<CdlProperty_String>(new_component->get_property(CdlPropertyId_Script));
            CYG_PRECONDITION_CLASSC(prop);
            std::string script_name = prop->get_string();

            // Try to locate this script.
            std::string script_filename = package->find_absolute_file(script_name, "cdl", false);
            if ("" == script_filename) {
                CdlParse::report_error(interp, "", "Unable to find script `" + script_name + "'.");
            } else {
                // The script exists, so we need to try and execute it.
                // The current container is still set correctly, but we need
                // to change the filename and install a different set
                // of commands.
                old_context = interp->push_context(script_filename);
                new_commands.clear();
                for (i = 0; 0 != script_commands[i].command; i++) {
                    new_commands.push_back(script_commands[i]);
                }
                old_commands = interp->push_commands(new_commands);
                result = interp->eval_file(script_filename, tcl_result);
                interp->pop_commands(old_commands);
                interp->pop_context(old_context);
            }
        }

      done2:
        // Dummy command just to keep the compiler happy
        old_context = "";
        
    } catch (std::bad_alloc e) {
        // Errors at this stage should be reported via Tcl, not via C++.
        // However there is no point in continuing with the parsing operation,
        // just give up.
        interp->set_result(CdlParse::construct_diagnostic(interp, "internal error", "", "Out of memory"));
        result = TCL_ERROR;
    } catch (CdlParseException e) {
        interp->set_result(e.get_message());
        result = TCL_ERROR;
    } catch(...) {
        interp->set_result(CdlParse::construct_diagnostic(interp, "internal error", "", "Unexpected C++ exception"));
        result = TCL_ERROR;
    }

    // Restore the interpreter to its prior state.
    interp->pop_node(old_node);
    interp->pop_container(old_container);
    if (0 != old_commands) {
        interp->pop_commands(old_commands);
    }
    
    CYG_REPORT_RETVAL(result);
    return result;
}


// ----------------------------------------------------------------------------
// Syntax: script <filename>
int
CdlComponentBody::parse_script(CdlInterpreter interp, int argc, const char* argv[])
{
    CYG_REPORT_FUNCNAMETYPE("parse_script", "result %d");

    int result = CdlParse::parse_string_property(interp, argc, argv, CdlPropertyId_Script, 0, 0);
    
    CYG_REPORT_RETVAL(result);
    return result;
}

//}}}
//{{{  Propagation support              

// ----------------------------------------------------------------------------
void
CdlComponentBody::update(CdlTransaction transaction, CdlUpdate update)
{
    CYG_REPORT_FUNCNAME("CdlComponent::update");

    this->CdlValuableBody::update(transaction, update);
    this->CdlContainerBody::update(transaction, update);
    
    CYG_REPORT_RETURN();
}

//}}}
//{{{  Persistence support              

// ----------------------------------------------------------------------------
void
CdlComponentBody::initialize_savefile_support(CdlToplevel toplevel)
{
    CYG_REPORT_FUNCNAME("CdlComponent::initialize_savefile_support");

    toplevel->add_savefile_command("cdl_component", 0, &savefile_component_command);
    CdlValuableBody::initialize_savefile_support(toplevel, "cdl_component");

    CYG_REPORT_RETURN();
}

void
CdlComponentBody::save(CdlInterpreter interp, Tcl_Channel chan, int indentation, bool minimal)
{
    CYG_REPORT_FUNCNAME("CdlComponent::save");
    CYG_REPORT_FUNCARG5XV(this, interp, chan, indentation, minimal);
    CYG_PRECONDITION_THISC();
    CYG_PRECONDITION_CLASSC(interp);

    if (!minimal || this->has_additional_savefile_information() || this->value_savefile_entry_needed()) {
        // Start with the UserVisible data, which will result in a suitable set
        // of comments before the package definition itself.
        this->CdlUserVisibleBody::save(interp, chan, indentation, minimal);

        // Now output the line "cdl_component <name> {"
        // The name is guaranteed to be a valid C preprocessor symbol, so it
        // is not going to need any quoting.
        std::string data = std::string(indentation, ' ') + "cdl_component " + get_name() + " {\n";
        interp->write_data(chan, data);

        // Deal with the value
        bool modifiable = !(CdlValueFlavor_None == this->get_flavor()) &&
            !this->has_property(CdlPropertyId_Calculated);
        this->CdlValuableBody::save(interp, chan, indentation + 4, modifiable, minimal);

        // And with any unrecognised data
        this->CdlNodeBody::save(interp, chan, indentation + 4, minimal);
    
        // Close the cdl_component body. A blank line is added here.
        interp->write_data(chan, "};\n\n");
    }
    
    // Packages are containers, so dump the contents as well.
    this->CdlContainerBody::save(interp, chan, indentation, minimal);
    
    CYG_REPORT_RETURN();
}

int
CdlComponentBody::savefile_component_command(CdlInterpreter interp, int argc, const char* argv[])
{
    CYG_REPORT_FUNCNAMETYPE("CdlComponent::savefile_component_command", "result %d");
    CYG_PRECONDITION_CLASSC(interp);

    int result = TCL_OK;
    CdlToplevel toplevel = interp->get_toplevel();
    CYG_ASSERT_CLASSC(toplevel);
    CdlConfiguration config = dynamic_cast<CdlConfiguration>(toplevel);
    CYG_ASSERT_CLASSC(config);

    std::vector<CdlInterpreterCommandEntry> subcommands;
    std::vector<CdlInterpreterCommandEntry>* toplevel_commands = 0;
    CdlNode old_node = 0;
    
    try {
        
        if (3 != argc) {
            CdlParse::report_error(interp, "", "Invalid cdl_component command in savefile, expecting two arguments.");
        } else {

            CdlNode current_node = config->lookup(argv[1]);
            if (0 == current_node) {
                // FIXME: save value in limbo
                CdlParse::report_error(interp, "", 
                                       std::string("The savefile contains a cdl_component command for an unknown component `")
                                       + argv[1] + "'");
            } else {
                config->get_savefile_subcommands("cdl_component", subcommands);
                toplevel_commands = interp->push_commands(subcommands);
                old_node = interp->push_node(current_node);
                
                std::string tcl_result;
                result = interp->eval(argv[2], tcl_result);
            
                interp->pop_commands(toplevel_commands);
                toplevel_commands = 0;
                interp->pop_node(old_node);
                old_node = 0;
            }
        }
    } catch(...) {
        if (0 != old_node) {
            interp->pop_node(old_node);
        }
        if (0 != toplevel_commands) {
            interp->pop_commands(toplevel_commands);
        }
        throw;
    }

    CYG_REPORT_RETVAL(result);
    return result;
}

//}}}
//{{{  check_this()                     

// ----------------------------------------------------------------------------

bool
CdlComponentBody::check_this(cyg_assert_class_zeal zeal) const
{
    if (CdlComponentBody_Magic != cdlcomponentbody_cookie) {
        return false;
    }
    CYGDBG_MEMLEAK_CHECKTHIS();

    return CdlNodeBody::check_this(zeal)        &&
           CdlContainerBody::check_this(zeal)   &&
           CdlUserVisibleBody::check_this(zeal) &&
           CdlParentableBody::check_this(zeal)  &&
           CdlValuableBody::check_this(zeal)    &&
           CdlBuildableBody::check_this(zeal)   &&
           CdlDefinableBody::check_this(zeal);
}

//}}}
//{{{  Misc                             

// ----------------------------------------------------------------------------

std::string
CdlComponentBody::get_class_name() const
{
    CYG_REPORT_FUNCNAME("CdlComponent::get_class_name");
    CYG_PRECONDITION_THISC();
    CYG_REPORT_RETURN();
    return "component";
}

//}}}
