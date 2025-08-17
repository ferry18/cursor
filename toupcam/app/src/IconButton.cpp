#include "IconButton.h"

IconButton::IconButton(QWidget *parent)
    : QPushButton(parent)
    , m_currentState(0)
    , m_stateCount(1)
{
    // Set button style
    setFixedSize(48, 48);
    setIconSize(QSize(32, 32));
    setFlat(true);
    
    setStyleSheet(
        "QPushButton {"
        "   background-color: transparent;"
        "   border: 1px solid #555;"
        "   border-radius: 8px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #444;"
        "   border-color: #777;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #333;"
        "   border-color: #888;"
        "}"
    );
    
    connect(this, &QPushButton::clicked, this, [this]() {
        nextState();
    });
}

void IconButton::addStateIcon(int state, const QString& iconPath)
{
    // Ensure vector is large enough
    if (state >= static_cast<int>(m_stateIcons.size())) {
        m_stateIcons.resize(state + 1);
    }
    
    m_stateIcons[state] = QIcon(iconPath);
    
    // Update icon if this is the current state
    if (state == m_currentState) {
        updateIcon();
    }
}

void IconButton::setState(int state)
{
    if (state != m_currentState && state >= 0 && state < m_stateCount) {
        m_currentState = state;
        updateIcon();
        emit stateChanged(m_currentState);
    }
}

void IconButton::nextState()
{
    int newState = (m_currentState + 1) % m_stateCount;
    setState(newState);
}

void IconButton::updateIcon()
{
    if (m_currentState < static_cast<int>(m_stateIcons.size())) {
        setIcon(m_stateIcons[m_currentState]);
    }
}