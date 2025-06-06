/***************************************************************************
 *   Copyright (c) 2008 Jürgen Riegel <juergen.riegel@web.de>              *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/

#include "PreCompiled.h"
#ifndef _PreComp_
#include <QDateTime>
#include <boost/random.hpp>
#include <cmath>
#endif

#include <Base/Reader.h>
#include <Base/Tools.h>
#include <Base/Writer.h>

#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include "Constraint.h"
#include "ConstraintPy.h"


using namespace Sketcher;
using namespace Base;


TYPESYSTEM_SOURCE(Sketcher::Constraint, Base::Persistence)

Constraint::Constraint()
    : Value(0.0)
    , Type(None)
    , AlignmentType(Undef)
    , First(GeoEnum::GeoUndef)
    , FirstPos(PointPos::none)
    , Second(GeoEnum::GeoUndef)
    , SecondPos(PointPos::none)
    , Third(GeoEnum::GeoUndef)
    , ThirdPos(PointPos::none)
    , LabelDistance(10.f)
    , LabelPosition(0.f)
    , isDriving(true)
    , InternalAlignmentIndex(-1)
    , isInVirtualSpace(false)
    , isActive(true)
{
    // Initialize a random number generator, to avoid Valgrind false positives.
    // The random number generator is not threadsafe so we guard it.  See
    // https://www.boost.org/doc/libs/1_62_0/libs/uuid/uuid.html#Design%20notes
    static boost::mt19937 ran;
    static bool seeded = false;
    static boost::mutex random_number_mutex;

    boost::lock_guard<boost::mutex> guard(random_number_mutex);

    if (!seeded) {
        ran.seed(QDateTime::currentMSecsSinceEpoch() & 0xffffffff);
        seeded = true;
    }
    static boost::uuids::basic_random_generator<boost::mt19937> gen(&ran);

    tag = gen();
}

Constraint* Constraint::clone() const
{
    return new Constraint(*this);
}

Constraint* Constraint::copy() const
{
    Constraint* temp = new Constraint();
    temp->Value = this->Value;
    temp->Type = this->Type;
    temp->AlignmentType = this->AlignmentType;
    temp->Name = this->Name;
    temp->First = this->First;
    temp->FirstPos = this->FirstPos;
    temp->Second = this->Second;
    temp->SecondPos = this->SecondPos;
    temp->Third = this->Third;
    temp->ThirdPos = this->ThirdPos;
    temp->LabelDistance = this->LabelDistance;
    temp->LabelPosition = this->LabelPosition;
    temp->isDriving = this->isDriving;
    temp->InternalAlignmentIndex = this->InternalAlignmentIndex;
    temp->isInVirtualSpace = this->isInVirtualSpace;
    temp->isActive = this->isActive;
    // Do not copy tag, otherwise it is considered a clone, and a "rename" by the expression engine.
    return temp;
}

PyObject* Constraint::getPyObject()
{
    return new ConstraintPy(new Constraint(*this));
}

Quantity Constraint::getPresentationValue() const
{
    Quantity quantity;
    switch (Type) {
        case Distance:
        case Radius:
        case Diameter:
        case DistanceX:
        case DistanceY:
            quantity.setValue(Value);
            quantity.setUnit(Unit::Length);
            break;
        case Angle:
            quantity.setValue(toDegrees<double>(Value));
            quantity.setUnit(Unit::Angle);
            break;
        case SnellsLaw:
        case Weight:
            quantity.setValue(Value);
            break;
        default:
            quantity.setValue(Value);
            break;
    }

    QuantityFormat format = quantity.getFormat();
    format.option = QuantityFormat::None;
    format.format = QuantityFormat::Default;
    format.precision = 6;  // QString's default
    quantity.setFormat(format);
    return quantity;
}

unsigned int Constraint::getMemSize() const
{
    return 0;
}

void Constraint::Save(Writer& writer) const
{
    std::string encodeName = encodeAttribute(Name);
    writer.Stream() << writer.ind() << "<Constrain "
                    << "Name=\"" << encodeName << "\" "
                    << "Type=\"" << (int)Type << "\" ";
    if (this->Type == InternalAlignment) {
        writer.Stream() << "InternalAlignmentType=\"" << (int)AlignmentType << "\" "
                        << "InternalAlignmentIndex=\"" << InternalAlignmentIndex << "\" ";
    }
    writer.Stream() << "Value=\"" << Value << "\" "
                    << "First=\"" << First << "\" "
                    << "FirstPos=\"" << (int)FirstPos << "\" "
                    << "Second=\"" << Second << "\" "
                    << "SecondPos=\"" << (int)SecondPos << "\" "
                    << "Third=\"" << Third << "\" "
                    << "ThirdPos=\"" << (int)ThirdPos << "\" "
                    << "LabelDistance=\"" << LabelDistance << "\" "
                    << "LabelPosition=\"" << LabelPosition << "\" "
                    << "IsDriving=\"" << (int)isDriving << "\" "
                    << "IsInVirtualSpace=\"" << (int)isInVirtualSpace << "\" "
                    << "IsActive=\"" << (int)isActive << "\" />"

                    << std::endl;
}

void Constraint::Restore(XMLReader& reader)
{
    reader.readElement("Constrain");
    Name = reader.getAttribute<const char*>("Name");
    Type = reader.getAttribute<ConstraintType>("Type");
    Value = reader.getAttribute<double>("Value");
    First = reader.getAttribute<long>("First");
    FirstPos = reader.getAttribute<PointPos>("FirstPos");
    Second = reader.getAttribute<long>("Second");
    SecondPos = reader.getAttribute<PointPos>("SecondPos");

    if (this->Type == InternalAlignment) {
        AlignmentType = reader.getAttribute<InternalAlignmentType>("InternalAlignmentType");

        if (reader.hasAttribute("InternalAlignmentIndex")) {
            InternalAlignmentIndex = reader.getAttribute<long>("InternalAlignmentIndex");
        }
    }
    else {
        AlignmentType = Undef;
    }

    // read the third geo group if present
    if (reader.hasAttribute("Third")) {
        Third = reader.getAttribute<long>("Third");
        ThirdPos = reader.getAttribute<PointPos>("ThirdPos");
    }

    // Read the distance a constraint label has been moved
    if (reader.hasAttribute("LabelDistance")) {
        LabelDistance = (float)reader.getAttribute<double>("LabelDistance");
    }

    if (reader.hasAttribute("LabelPosition")) {
        LabelPosition = (float)reader.getAttribute<double>("LabelPosition");
    }

    if (reader.hasAttribute("IsDriving")) {
        isDriving = reader.getAttribute<bool>("IsDriving");
    }

    if (reader.hasAttribute("IsInVirtualSpace")) {
        isInVirtualSpace = reader.getAttribute<bool>("IsInVirtualSpace");
    }

    if (reader.hasAttribute("IsActive")) {
        isActive = reader.getAttribute<bool>("IsActive");
    }
}

void Constraint::substituteIndex(int fromGeoId, int toGeoId)
{
    if (this->First == fromGeoId) {
        this->First = toGeoId;
    }
    if (this->Second == fromGeoId) {
        this->Second = toGeoId;
    }
    if (this->Third == fromGeoId) {
        this->Third = toGeoId;
    }
}

void Constraint::substituteIndexAndPos(int fromGeoId,
                                       PointPos fromPosId,
                                       int toGeoId,
                                       PointPos toPosId)
{
    if (this->First == fromGeoId && this->FirstPos == fromPosId) {
        this->First = toGeoId;
        this->FirstPos = toPosId;
    }
    if (this->Second == fromGeoId && this->SecondPos == fromPosId) {
        this->Second = toGeoId;
        this->SecondPos = toPosId;
    }
    if (this->Third == fromGeoId && this->ThirdPos == fromPosId) {
        this->Third = toGeoId;
        this->ThirdPos = toPosId;
    }
}

std::string Constraint::typeToString(ConstraintType type)
{
    return type2str[type];
}

std::string Constraint::internalAlignmentTypeToString(InternalAlignmentType alignment)
{
    return internalAlignmentType2str[alignment];
}
