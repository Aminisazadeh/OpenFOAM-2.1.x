/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 2011-2012 OpenFOAM Foundation
     \\/     M anipulation  |
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM.

    OpenFOAM is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OpenFOAM.  If not, see <http://www.gnu.org/licenses/>.

Application
    surfaceMeshImport

Description
    Import from various third-party surface formats into surfMesh
    with optional scaling or transformations (rotate/translate)
    on a coordinateSystem.

Usage
    - surfaceMeshImport inputFile [OPTION]

    \param -clean \n
    Perform some surface checking/cleanup on the input surface.

    \param -name \<name\> \n
    Specify an alternative surface name when writing.

    \param -scaleIn \<scale\> \n
    Specify a scaling factor when reading files.

    \param -scaleOut \<scale\> \n
    Specify a scaling factor when writing files.

    \param -dict \<dictionary\> \n
    Specify an alternative dictionary for constant/coordinateSystems.

    \param -from \<coordinateSystem\> \n
    Specify a coordinate system when reading files.

    \param -to \<coordinateSystem\> \n
    Specify a coordinate system when writing files.

Note
    The filename extensions are used to determine the file format type.

\*---------------------------------------------------------------------------*/

#include "argList.H"
#include "Time.H"

#include "MeshedSurfaces.H"
#include "coordinateSystems.H"

using namespace Foam;

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //
//  Main program:

int main(int argc, char *argv[])
{
    argList::addNote
    (
        "import from various third-party surface formats into surfMesh"
    );

    argList::noParallel();
    argList::validArgs.append("inputFile");

    argList::addBoolOption
    (
        "clean",
        "perform some surface checking/cleanup on the input surface"
    );
    argList::addOption
    (
        "name",
        "name",
        "specify an alternative surface name when writing - "
        "default is 'default'"
    );
    argList::addOption
    (
        "scaleIn",
        "factor",
        "geometry scaling factor on input - default is 1"
    );
    argList::addOption
    (
        "scaleOut",
        "factor",
        "geometry scaling factor on output - default is 1"
    );
    argList::addOption
    (
        "dict",
        "file",
        "specify an alternative dictionary for constant/coordinateSystems"
    );
    argList::addOption
    (
        "from",
        "coordinateSystem",
        "specify a local coordinate system when reading files."
    );
    argList::addOption
    (
        "to",
        "coordinateSystem",
        "specify a local coordinate system when writing files."
    );

    #include "setRootCase.H"
    #include "createTime.H"

    // try for the latestTime, but create "constant" as needed
    instantList Times = runTime.times();
    if (Times.size())
    {
        label startTime = Times.size()-1;
        runTime.setTime(Times[startTime], startTime);
    }
    else
    {
        runTime.setTime(instant(0, runTime.constant()), 0);
    }


    const fileName importName = args[1];
    const word exportName = args.optionLookupOrDefault<word>("name", "default");

    // check that reading is supported
    if (!MeshedSurface<face>::canRead(importName, true))
    {
        return 1;
    }


    // get the coordinate transformations
    autoPtr<coordinateSystem> fromCsys;
    autoPtr<coordinateSystem> toCsys;

    if (args.optionFound("from") || args.optionFound("to"))
    {
        autoPtr<IOobject> ioPtr;

        if (args.optionFound("dict"))
        {
            const fileName dictPath = args["dict"];

            ioPtr.set
            (
                new IOobject
                (
                    (
                        isDir(dictPath)
                      ? dictPath/coordinateSystems::typeName
                      : dictPath
                    ),
                    runTime,
                    IOobject::MUST_READ,
                    IOobject::NO_WRITE,
                    false
                )
            );
        }
        else
        {
            ioPtr.set
            (
                new IOobject
                (
                    coordinateSystems::typeName,
                    runTime.constant(),
                    runTime,
                    IOobject::MUST_READ,
                    IOobject::NO_WRITE,
                    false
                )
            );
        }


        if (!ioPtr->headerOk())
        {
            FatalErrorIn(args.executable())
                << "Cannot open coordinateSystems file\n    "
                << ioPtr->objectPath() << nl
                << exit(FatalError);
        }

        coordinateSystems csLst(ioPtr());

        if (args.optionFound("from"))
        {
            const word csName = args["from"];

            const label csIndex = csLst.findIndex(csName);
            if (csIndex < 0)
            {
                FatalErrorIn(args.executable())
                    << "Cannot find -from " << csName << nl
                    << "available coordinateSystems: " << csLst.toc() << nl
                    << exit(FatalError);
            }

            fromCsys.reset(new coordinateSystem(csLst[csIndex]));
        }

        if (args.optionFound("to"))
        {
            const word csName = args["to"];

            const label csIndex = csLst.findIndex(csName);
            if (csIndex < 0)
            {
                FatalErrorIn(args.executable())
                    << "Cannot find -to " << csName << nl
                    << "available coordinateSystems: " << csLst.toc() << nl
                    << exit(FatalError);
            }

            toCsys.reset(new coordinateSystem(csLst[csIndex]));
        }


        // maybe fix this later
        if (fromCsys.valid() && toCsys.valid())
        {
            FatalErrorIn(args.executable())
                << "Only allowed  '-from' or '-to' option at the moment."
                << exit(FatalError);
        }
    }



    MeshedSurface<face> surf(importName);

    if (args.optionFound("clean"))
    {
        surf.cleanup(true);
    }


    scalar scaleIn = 0;
    if (args.optionReadIfPresent("scaleIn", scaleIn) && scaleIn > 0)
    {
        Info<< " -scaleIn " << scaleIn << endl;
        surf.scalePoints(scaleIn);
    }

    if (fromCsys.valid())
    {
        Info<< " -from " << fromCsys().name() << endl;
        tmp<pointField> tpf = fromCsys().localPosition(surf.points());
        surf.movePoints(tpf());
    }

    if (toCsys.valid())
    {
        Info<< " -to " << toCsys().name() << endl;
        tmp<pointField> tpf = toCsys().globalPosition(surf.points());
        surf.movePoints(tpf());
    }

    scalar scaleOut = 0;
    if (args.optionReadIfPresent("scaleOut", scaleOut) && scaleOut > 0)
    {
        Info<< " -scaleOut " << scaleOut << endl;
        surf.scalePoints(scaleOut);
    }

    surfMesh smesh
    (
        IOobject
        (
            exportName,
            runTime.constant(),
            runTime
        ),
        surf.xfer()
    );


    Info<< "writing surfMesh:\n  " << smesh.objectPath() << endl;
    smesh.write();

    Info<< "\nEnd\n" << endl;

    return 0;
}

// ************************************************************************* //
