#pragma once

#include <QKeyEvent>

// Keyboard actions shared by the single viewer, the compare view and the grid.
enum class Action {
    None,
    Next, Prev, First, Last,
    Select, Reject, ClearMark,
    Rate,        // arg: 0..5
    Label,       // arg: Label enum value
    Keep,        // select this shot, reject the rest of its burst / compare set
    Back,
    Dive,        // open the burst under the current shot
    Compare,
    Rotate, Zoom, ToggleFocus,
};

struct KeyAction {
    Action action = Action::None;
    int arg = 0;
};

inline KeyAction keyAction(const QKeyEvent *event)
{
    const int key = event->key();
    if (key >= Qt::Key_0 && key <= Qt::Key_5) return {Action::Rate, key - Qt::Key_0};
    // 6..9 → Red, Yellow, Green, Blue (same keys as Lightroom).
    if (key >= Qt::Key_6 && key <= Qt::Key_9) return {Action::Label, key - Qt::Key_6 + 1};
    switch (key) {
    case Qt::Key_Right: return {Action::Next};
    case Qt::Key_Left: return {Action::Prev};
    case Qt::Key_Home: return {Action::First};
    case Qt::Key_End: return {Action::Last};
    case Qt::Key_Up:
    case Qt::Key_S: return {Action::Select};
    case Qt::Key_Down:
    case Qt::Key_X: return {Action::Reject};
    case Qt::Key_Space: return {Action::ClearMark};
    case Qt::Key_K: return {Action::Keep};
    case Qt::Key_Escape: return {Action::Back};
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_B: return {Action::Dive};
    case Qt::Key_C: return {Action::Compare};
    case Qt::Key_R: return {Action::Rotate};
    case Qt::Key_Z: return {Action::Zoom};
    case Qt::Key_F: return {Action::ToggleFocus};
    default: return {};
    }
}

// Actions that change marks must not fire repeatedly while a key is held down.
inline bool isEditAction(Action a)
{
    return a == Action::Select || a == Action::Reject || a == Action::ClearMark || a == Action::Rate
           || a == Action::Label || a == Action::Keep;
}
