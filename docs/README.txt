MuiBuilderQt - Windows-Build-Pipeline (windeployqt) + Installer-Script (.iss)
===============================================================================

Dieses Patch bringt MuiBuilderQt auf denselben Stand wie AmigaED in Sachen
Windows-Distribution: einen automatischen windeployqt-Schritt nach jedem
erfolgreichen Windows-Build, ein Staging der fertigen Dateien in einen
"install_src"-Ordner, sowie ein komplettes Inno-Setup-Installer-Script
(.iss), strukturell an AmigaEDs eigenes AmigaED.iss angelehnt.

Geänderte/neue Dateien
-----------------------
- gui/MuiBuilderQt.pro     (geändert: neuer win32{}-Block)
- gui/install_stage.bat    (neu: statisches Batch-Skript fürs Staging)
- MuiBuilderQt_install/MuiBuilderQt.iss  (neu: Installer-Script)

1. gui/MuiBuilderQt.pro - neuer win32{}-Block
-----------------------------------------------
Nach einem erfolgreichen Windows-Build (qmake6 + mingw32-make):
- DESTDIR wird auf $$OUT_PWD fixiert (kein unvorhersehbarer "release"-
  Unterordner mehr - genau wie bei AmigaED.pro).
- windeployqt.exe wird automatisch aufgerufen (über QT_INSTALL_BINS
  gefunden, nicht über PATH vorausgesetzt), kopiert alle benötigten
  Qt-DLLs/Plugins direkt neben die MuiBuilderQt.exe.
  MuiBuilderQt hat - anders als AmigaED (QScintilla) - KEINE Drittanbieter-
  DLL-Abhängigkeit, daher entfällt dieser Teil hier komplett.
- Ein Staging-Schritt kopiert danach alles (.exe + DLLs + Plugin-Ordner)
  sauber in <Projektwurzel>\MuiBuilderQt_install\install_src - das ist
  genau der Ordner, den das neue .iss-Script als SourceDir erwartet.

Wichtig: die eigentliche Kopierlogik (robocopy mit Ausschlussmustern)
steckt bewusst NICHT direkt in der .pro-Datei, sondern im separaten,
statischen install_stage.bat - das ist keine stilistische Entscheidung,
sondern eine aus AmigaED.pro übernommene Notwendigkeit: mingw32-make nutzt
für Build-Rezeptzeilen nicht zuverlässig cmd.exe als Shell - hat man den
m68k-amigaos-gcc-Cross-Compiler-Toolchain (oder eine andere sh.exe) im
PATH, wird STATTDESSEN dessen sh.exe verwendet, was Pfade/Wildcards direkt
in der .pro-Rezeptzeile zuverlässig zerschießt. Ein statisches .bat-File,
das nur als "cmd /c install_stage.bat <arg1> <arg2>" mit zwei einfachen,
wildcardfreien Pfaden aufgerufen wird, umgeht das vollständig (siehe
ausführliche Kommentare direkt in der .pro-Datei und im .bat-Skript).

Ein RC_ICONS-Eintrag für ein eigenes .exe-Icon ist als auskommentierte
Zeile vorbereitet (kein .ico-File liegt bisher im Projektbaum) - einfach
einkommentieren, sobald ein Icon existiert.

2. gui/install_stage.bat
--------------------------
Statisches Batch-Skript (vom Aufbau identisch zu AmigaEDs eigenem
install_stage.bat), leert/erstellt den Zielordner neu und kopiert den
Build-Output per robocopy hinein (Build-Artefakte wie Makefile*, .rc/.h-
Dateien und der "release"-Unterordner werden ausgeschlossen). Anders als
bei AmigaED gibt es KEIN zusätzliches Staging von DOC-/Examples-Ordnern -
MuiBuilderQt hat keine entsprechenden Assets.

3. MuiBuilderQt_install/MuiBuilderQt.iss
-------------------------------------------
Komplettes Inno-Setup-Script, strukturell an AmigaED.iss angelehnt:
- Frisch generierte, eigene AppId-GUID (3C3B3F82-576C-4F5B-9CA7-
  6076C2EAEC67) - garantiert NICHT identisch mit AmigaEDs eigener AppId.
- DefaultDirName=C:\MuiBuilderQt - bewusst passend zum bereits in AmigaED
  hinterlegten Default-Pfad für "Werkzeug 1" (MUI GUI Designer) im neuen
  Prefs-Reiter "Tools" (siehe vorheriges Tools-Integration-Patch) - eine
  frische MuiBuilderQt-Installation über dieses Script "passt" also ohne
  weiteres Zutun zur AmigaED-Standardkonfiguration.
- Deutsch/Englisch als Sprachen (wie bei AmigaED).
- Derselbe "Bereits installiert?"-Auswahldialog (Neu installieren/Update/
  Deinstallieren/Abbrechen) wie bei AmigaED - Mechanik 1:1 übernommen.
- Ein Lizenz-/Hinweis-Dialog vor dem eigentlichen Setup (Accept/Cancel) -
  Mechanik von AmigaEDs Disclaimer-Dialog übernommen, TEXT aber bewusst
  NEU und NEUTRAL formuliert (siehe wichtiger Hinweis unten).
- Kein Dokumentations-/Beispielprojekte-Abfrage-Mechanismus (AmigaED-
  spezifisch, für MuiBuilderQt entfernt - es gibt keine entsprechenden
  Ordner).
- Optionales wizard_image.png/wizard_image_small.png (wie bei AmigaED,
  nur wirksam falls diese Dateien tatsächlich neben der .iss liegen).

WICHTIGER HINWEIS zum Lizenz-/Hinweis-Text
---------------------------------------------
AmigaEDs eigener Disclaimer-Dialog enthält einen persönlichen, politisch
gefärbten Text des AmigaED-Autors. Dieser wurde bewusst NICHT 1:1 in die
neue MuiBuilderQt.iss übernommen (das wäre eine eigenmächtige inhaltliche
Entscheidung für ein anderes Projekt gewesen) - stattdessen enthält der
neue Dialog einen rein sachlichen Text: Verweis auf die Herkunft (Qt6/C++-
Port von Darren Coles' MUI-Builder v2.3, mit dessen Erlaubnis entstanden)
plus "ohne jede Gewährleistung". Es wird KEINE Lizenzform behauptet, die
es im MuiBuilderQt-Repository bisher nicht gibt. Der Text lässt sich in
[CustomMessages] (DisclaimerText, Englisch + Deutsch) frei anpassen, falls
ein anderer Wortlaut gewünscht ist.

Weitere offene Punkte / Annahmen (bewusst mit sinnvollen Platzhaltern
gefüllt statt zu blockieren)
------------------------------------------------------------------------
- MyAppVersion ist auf "1.0" gesetzt - MuiBuilderQt hat noch kein
  etabliertes Versionsschema (kein version.h o.ä. im Projekt). Einfach in
  MuiBuilderQt.iss (#define MyAppVersion) anpassen, sobald ein Schema
  feststeht.
- OutputBaseFilename ist "MuiBuilderQt_Setup" (kein Revisions-/Build-
  Nummern-Schema wie bei AmigaEDs "AmigaED4_rev157_Setup" vorhanden).
- Kein .exe-Icon eingebettet (kein .ico-File im Projektbaum vorhanden -
  RC_ICONS-Zeile liegt bereits vorbereitet/auskommentiert in
  gui/MuiBuilderQt.pro).

Verifikation
------------
- gui/MuiBuilderQt.pro: sauberer qmake6-Parse + vollständiger, sauberer
  Linux/Qt6-Build (g++, keine Fehler, keine Warnungen) als Regressionstest
  - der neue win32{}-Block selbst wird unter Linux nicht ausgeführt (wie
  bei AmigaED.pro), muss aber syntaktisch gültig bleiben und darf den
  restlichen Build nicht stören - beides bestätigt.
- MuiBuilderQt_install/MuiBuilderQt.iss: in dieser Sandbox steht kein
  Windows/MinGW-Toolchain und kein Inno-Setup-Compiler (ISCC) zur
  Verfügung - das Script konnte daher NICHT kompiliert/getestet werden.
  Stattdessen wurde es sorgfältig Zeile für Zeile strukturell gegen das
  bekannt funktionierende AmigaED.iss verglichen (identische Pascal-
  Script-Mechanik für Auswahl-/Disclaimer-Dialog, [Setup]/[Files]/[Icons]/
  [Run]-Sektionen 1:1 in der Struktur übernommen, nur AmigaED-spezifische
  Teile entfernt) sowie eine einfache Balance-Prüfung (begin/end-Zählung)
  gegen das Original durchgeführt - beide Dateien zeigen dieselbe
  strukturelle Differenz (3 "end" ohne zugehöriges "begin", jeweils durch
  try/finally- und case-Blöcke erklärt), was für strukturelle Konsistenz
  spricht. Ein echter ISCC-Kompilierlauf unter Windows bleibt trotzdem
  empfohlen, bevor das Script produktiv verwendet wird - genau wie schon
  beim früheren AmigaED-Windows-Icon-Feature (dort ebenfalls nur qmake-
  Parsing + Linux-Build gegengeprüft, da kein MinGW verfügbar war).

Installation
------------
Diese drei Dateien im lokalen MuiBuilderQt-Projektverzeichnis übernehmen
(gui/MuiBuilderQt.pro ersetzen bzw. per Diff/Merge einspielen, gui/
install_stage.bat und MuiBuilderQt_install/MuiBuilderQt.iss neu anlegen),
danach unter Windows mit qmake6 + mingw32-make neu bauen - windeployqt und
das Staging laufen automatisch mit. Für den eigentlichen Installer
anschließend MuiBuilderQt_install/MuiBuilderQt.iss mit dem Inno Setup
Compiler (ISCC.exe) übersetzen - das Ergebnis landet in
MuiBuilderQt_install/Output/MuiBuilderQt_Setup.exe.
