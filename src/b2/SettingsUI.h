#ifndef HEADER_933F80A7CAAF44EEBA8D2EC51C7A2A6E// -*- mode:c++ -*-
#define HEADER_933F80A7CAAF44EEBA8D2EC51C7A2A6E

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class SettingsUI {
public:
    SettingsUI();
    virtual ~SettingsUI()=0;

    SettingsUI(const SettingsUI &)=delete;
    SettingsUI &operator=(const SettingsUI &)=delete;
    SettingsUI(SettingsUI &&)=delete;
    SettingsUI &operator=(SettingsUI &&)=delete;

    virtual void DoImGui()=0;

    virtual bool DidConfigChange() const=0;

    // default impl returns false.
    virtual bool WantsKeyboardFocus() const;
protected:
private:
};

#endif
