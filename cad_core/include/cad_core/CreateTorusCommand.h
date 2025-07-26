#pragma once

#include "ICommand.h"
#include "Shape.h"
#include "Point.h"

namespace cad_core {

class CreateTorusCommand : public ICommand {
public:       
    CreateTorusCommand(const Point& center, double majorRadius, double minorRadius);
    virtual ~CreateTorusCommand() = default;

        
    bool Execute() override;
    bool Undo() override;
    bool Redo() override;
    const char* GetName() const override;

    ShapePtr GetCreatedShape() const;

private:
    Point m_center;
    double m_majorRadius;
    double m_minorRadius;
    ShapePtr m_createdShape;
    bool m_executed;
};

} // namespace cad_core
