#include <shared/system.h>
#include <shared/debug.h>
#include "commands.h"
#include "dear_imgui.h"
#include "keys.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::map<std::string,CommandTable *> *g_all_command_tables;

static void InitAllCommandTables() {
    if(!g_all_command_tables) {
        static std::map<std::string,CommandTable *> s_all_command_tables;

        g_all_command_tables=&s_all_command_tables;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Command::Command(std::string name,std::string text,bool confirm):
    m_name(std::move(name)),
    m_text(std::move(text)),
    m_confirm(confirm)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const std::string &Command::GetName() const {
    return m_name;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const std::string &Command::GetText() const {
    return m_text;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CommandTable::ForEachCommandTable(std::function<void(CommandTable *)> fun) {
    InitAllCommandTables();

    for(auto &&it:*g_all_command_tables) {
        fun(it.second);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CommandTable *CommandTable::FindCommandTableByName(const std::string &name) {
    InitAllCommandTables();

    auto &&it=g_all_command_tables->find(name);
    if(it==g_all_command_tables->end()) {
        return nullptr;
    } else {
        return it->second;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CommandTable::CommandTable(std::string name):
    m_keymap(std::move(name),true)
{
    InitAllCommandTables();

    ASSERT(g_all_command_tables->count(this->GetName())==0);
    (*g_all_command_tables)[this->GetName()]=this;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CommandTable::~CommandTable() {
    ASSERT((*g_all_command_tables)[this->GetName()]==this);
    g_all_command_tables->erase(this->GetName());
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Command *CommandTable::FindCommandByName(const char *name) const {
    auto &&it=m_command_by_name.find(name);
    if(it!=m_command_by_name.end()) {
        return it->second.get();
    } else {
        return nullptr;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Command *CommandTable::FindCommandByName(const std::string &str) const {
    return this->FindCommandByName(str.c_str());
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const std::string &CommandTable::GetName() const {
    return m_keymap.GetName();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CommandTable::ForEachCommand(std::function<void(Command *)> fun) {
    UpdateSortedCommands();

    for(size_t i=0;i<m_commands_sorted.size();++i) {
        fun(m_commands_sorted[i]);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CommandTable::ClearMappingsByCommand(Command *command) {
    m_keymap.ClearMappingsByValue(command);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CommandTable::SetMapping(uint32_t pc_key,Command *command,bool state) {
    ASSERT(this->FindCommandByName(command->m_name));

    m_keymap.SetMapping(pc_key,command,state);

    if(state) {
        if(command->m_shortcut==0) {
            command->m_shortcut=pc_key;
        }
    } else {
        const uint32_t *pc_keys=m_keymap.GetPCKeysForValue(command);
        if(pc_keys) {
            command->m_shortcut=pc_keys[0];
        } else {
            command->m_shortcut=0;
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const uint32_t *CommandTable::GetPCKeysForValue(Command *command) const {
    return m_keymap.GetPCKeysForValue(command);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const Command *const *CommandTable::GetValuesForPCKey(uint32_t key) const {
    return m_keymap.GetValuesForPCKey(key);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Command *CommandTable::AddCommand(std::unique_ptr<Command> command) {
    ASSERT(!this->FindCommandByName(command->m_name));

    Command *result=command.get();

    m_command_by_name[command->m_name.c_str()]=std::move(command);
    m_commands_sorted_dirty=true;

    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CommandTable::UpdateSortedCommands() {
    if(m_commands_sorted_dirty) {
        m_commands_sorted.clear();
        m_commands_sorted.reserve(m_command_by_name.size());
        for(auto &&it:m_command_by_name) {
            m_commands_sorted.push_back(it.second.get());
        }

        std::sort(m_commands_sorted.begin(),m_commands_sorted.end(),[](auto a,auto b) {
            return a->GetText()<b->GetText();
        });

        m_commands_sorted_dirty=false;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CommandContext::CommandContext(void *object,const CommandTable *table):
    m_object(object),
    m_table(table)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CommandContext::Reset() {
    m_object=nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CommandContext::DoButton(const char *name) {
    Command *command=m_table->FindCommandByName(name);
    if(!command) {
        return;
    }

    if(!m_object) {
        return;
    }

    //bool enabled=command->IsEnabled(m_object);

    if(command->m_confirm) {
        // bleargh...
    } else if(const bool *ticked=command->IsTicked(m_object)) {
        if(ImGui::RadioButton(command->m_text.c_str(),*ticked)) {
            command->Execute(m_object);
        }
    } else {
        if(ImGui::Button(command->m_text.c_str())) {
            command->Execute(m_object);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CommandContext::DoMenuItemUI(const char *name) {
    Command *command=m_table->FindCommandByName(name);
    if(!command) {
        return;
    }

    if(!m_object) {
        return;
    }

    std::string shortcut;
    if(command->m_shortcut!=0) {
        shortcut=GetKeycodeName(command->m_shortcut).c_str();
    }

    bool enabled=command->IsEnabled(m_object);

    if(command->m_confirm) {
        if(ImGui::BeginMenu(command->m_text.c_str(),enabled)) {
            if(ImGui::MenuItem("Confirm")) {
                command->Execute(m_object);
            }
            ImGui::EndMenu();
        }
    } else {
        const bool *ticked=command->IsTicked(m_object);

        bool selected=false;
        if(ticked&&*ticked) {
            selected=true;
        }

        if(ImGui::MenuItem(command->m_text.c_str(),shortcut.empty()?nullptr:shortcut.c_str(),&selected,enabled)) {
            command->Execute(m_object);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool CommandContext::ExecuteCommandsForPCKey(uint32_t keycode) const {
    const Command *const *commands=m_table->GetValuesForPCKey(keycode);
    if(!commands) {
        return false;
    }

    if(!m_object) {
        return false;
    }

    for(size_t i=0;commands[i];++i) {
        commands[i]->Execute(m_object);
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const CommandTable *CommandContext::GetCommandTable() const {
    return m_table;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CommandContextStack::Push(const std::shared_ptr<CommandContext> &cc,bool force) {
    if(force||ImGui::IsRootWindowFocused()) {
        m_ccs.push_back(cc);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CommandContextStack::Reset() {
    m_ccs.clear();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t CommandContextStack::GetNumCCs() const {
    return m_ccs.size();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const std::shared_ptr<CommandContext> &CommandContextStack::GetCCByIndex(size_t index) const {
    ASSERT(index<m_ccs.size());
    return m_ccs[m_ccs.size()-1-index];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool CommandContextStack::ExecuteCommandsForPCKey(uint32_t keycode) const {
    for(size_t i=0;i<this->GetNumCCs();++i) {
        const std::shared_ptr<CommandContext> &cc=this->GetCCByIndex(i);

        if(cc->ExecuteCommandsForPCKey(keycode)) {
            return true;
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
