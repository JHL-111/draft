#include "cad_core/CreateTorusCommand.h"
#include "cad_core/ShapeFactory.h" 
namespace cad_core {

CreateTorusCommand::CreateTorusCommand(const Point& center, double majorRadius, double minorRadius)
    : m_center(center),m_majorRadius(majorRadius),m_minorRadius(minorRadius),m_executed(false) { 
}

bool CreateTorusCommand::Execute() {
    if (m_executed) {
        return true;
    }

    m_createdShape = ShapeFactory::CreateTorus(m_center, m_majorRadius, m_minorRadius);
    m_executed = (m_createdShape != nullptr);
    return m_executed;
}

bool CreateTorusCommand::Undo() {
    if (!m_executed) {
        return false;
    }

    m_createdShape.reset(); 
    m_executed = false;
    return true;
}

bool CreateTorusCommand::Redo() {
    if (m_executed) {
        return true;
    }

    return Execute();
}

const char* CreateTorusCommand::GetName() const {
    return "Create Torus";
}

ShapePtr CreateTorusCommand::GetCreatedShape() const {
    return m_createdShape;
}

} // namespace cad_core