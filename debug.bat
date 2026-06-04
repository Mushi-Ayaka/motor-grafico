@echo off
:: Test that the .rih file can be resolved
:: Path: ..\..\Lenguaje Hermetico\ejemplos\bodegon.rih
dir "..\..\Lenguaje Hermetico\ejemplos\bodegon.rih"
if exist "..\..\Lenguaje Hermetico" (
    echo DIR exists: "..\..\Lenguaje Hermetico"
) else (
    echo DIR NOT FOUND
)
if exist "..\..\Lenguaje Hermetico\ejemplos\bodegon.rih" (
    echo RIH FILE EXISTS
) else (
    echo RIH FILE NOT FOUND
)
pause
