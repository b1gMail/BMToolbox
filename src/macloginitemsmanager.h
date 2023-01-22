#ifndef MACLOGINITEMSMANAGER_H
#define MACLOGINITEMSMANAGER_H

class MacLoginItemsManager
{
public:
    MacLoginItemsManager();
    ~MacLoginItemsManager();
    bool appendRunningApplication() const;
    bool removeRunningApplication() const;
    bool containsRunningApplication() const;

private:
    bool findRunningApplication(bool remove) const;

    class Private;
    Private *d;
};

class MacHelper
{
public:
    static void launchUpdater(const char *path);
};

#endif // MACLOGINITEMSMANAGER_H
