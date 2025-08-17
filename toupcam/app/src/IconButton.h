#ifndef ICONBUTTON_H
#define ICONBUTTON_H

#include <QPushButton>
#include <QIcon>
#include <vector>

class IconButton : public QPushButton
{
    Q_OBJECT

public:
    explicit IconButton(QWidget *parent = nullptr);
    
    // Add icon for a specific state
    void addStateIcon(int state, const QString& iconPath);
    
    // Set current state
    void setState(int state);
    int getState() const { return m_currentState; }
    
    // For cycling through states
    void setStateCount(int count) { m_stateCount = count; }
    void nextState();
    
signals:
    void stateChanged(int newState);

private:
    void updateIcon();
    
private:
    std::vector<QIcon> m_stateIcons;
    int m_currentState;
    int m_stateCount;
};

#endif // ICONBUTTON_H