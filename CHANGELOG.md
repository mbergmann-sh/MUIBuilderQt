# MuiBuilderQt - Zwischenstand (Qt6/C++-Port von MUI-Builder v2.3)

Dies ist ein Sicherheits-Snapshot des aktuellen, funktionsfaehigen
Zwischenstands - kein fertiges Endprodukt.

**Update:** Die interaktive GUI (Widget-Palette, Drag&Drop-Canvas,
Property-Inspector, Projektbaum) ist jetzt hinzugekommen - siehe den
neuen Abschnitt "Interaktive GUI (gui/)" unten. Die Core-Engine (siehe
naechster Abschnitt) war bereits vorher fertig und validiert.

## Herkunft / Berechtigung

Portierung von Darren Coles' MUI-Builder (v2.3, C-Quellcode via SVN-Checkout
"muibuilder-code.7z" vom Nutzer bereitgestellt) nach Qt6/C++, mit Erlaubnis
von Darren Coles fuer dieses Teilprojekt. Teil des geplanten "user-defined
Tools"-Menuerweiterung fuer AmigaED (bewusst als eigenstaendiges externes Tool
entworfen, nicht in AmigaED eingebettet).

## Was fertig und validiert ist

- **core/muibobject.h** - vollstaendiges Objektbaum-Datenmodell (alle
  24 Objekttypen: Window, Group, Button, Text, String, Listview, Label,
  Cycle, Radio, Check, Space, Rectangle, Gauge, Slider, Prop, Image,
  ColorField, PopAsl, PopObject, Menu, SubMenu, MenuItem, sowie
  Application/Project-Ebene).
- **core/muibloader.h/.cpp** - Laden von `.MUIB`-Dateien (Port von `load.c`).
- **core/muibsaver.h/.cpp** - Speichern von `.MUIB`-Dateien (Port von
  `save.c`), inkl. korrektem Format-Upgrade beim Speichern aelterer
  Dateiversionen (synthetisiert leere Menu-Objekte, wenn das Original sie
  unconditional erwartet - siehe Kommentare im Code).
- **core/muicodegen.h/.cpp** - C-Codegenerator (siehe CODEGEN_NOTES.md fuer
  die vollstaendige Design-Begruendung). WICHTIG: dies ist KEIN
  byte-identischer Nachbau des Original-Generators - eine dafuer noetige
  Tabelle (`MUIStrings[]`) fehlt im bereitgestellten Quellbaum. Stattdessen:
  ein sauberer, direkter Generator, der echte MUI-SDK-Makros verwendet und
  strukturell korrekten, funktionsfaehigen MUI-C-Code erzeugt. Erzeugt pro
  Projekt jetzt VIER Dateien statt zwei - `<baseName>.h`, `<baseName>.c`,
  `<baseName>_gadgets.h`, `<baseName>_main.c`:
  - **`<baseName>_main.c`** (`generateMain()`) - das komplette, lauffaehige
    Hauptprogramm: `muimaster.library` oeffnen, `BuildApplication()`
    aufrufen, jedes Fenster mit gesetztem `initopen`
    ("Initial geoeffnet" im Inspector) oeffnen (hat kein Fenster dieses
    Flag, wird ersatzweise das erste geoeffnet, mit erklaerendem
    Kommentar im generierten Code), dann die Standard-MUI-Ereignisschleife
    (`DoMethod(app, MUIM_Application_NewInput, &sigs)` + `Wait()`, mit
    einem `switch`, der `MUIV_Application_ReturnID_Quit` behandelt und
    Platz fuer eigene Notify-IDs kommentiert vorbereitet) - dieselbe
    Schleife bedient Fenster-Schliessen, Menüpunkte UND Gadgets
    gleichermassen, da MUI alle Notify-Rueckgabewerte darueber liefert.
    Am Original orientiert (`GenCodeC.c`s `WriteMainFile()` hat exakt
    diese Form: `OpenLibrary`/Event-Loop/`Wait(sigs | SIGBREAKF_CTRL_C)`),
    aber an unsere eigene, flache `BuildApplication()`/`Gui`-Struktur
    angepasst statt am Original-Tool's separatem `ObjApp`-Rahmenwerk
    (`CreateApp()`/`DisposeApp()`), das im bereitgestellten Quellbaum
    fehlt.
  - **`<baseName>_gadgets.h`** (`generateGadgetStubs()`) - ein
    Debug-Funktions-Stub pro benanntem "echten" Gadget im Projekt (jeder
    Button/String/Cycle/... - nicht Window/Group/Space/Rectangle und
    nicht die Menu-Familie), automatisch von `<baseName>.c` inkludiert:
    `static inline void <Ident>_Clicked(void) { if (myDebug) { puts("Ich
    bin <Ident> und wurde geklickt"); } }` (`myDebug` als globaler
    An/Aus-Schalter, in `_main.c` definiert). `static inline` verhindert
    `-Wunused-function`, solange ein Stub noch nicht tatsaechlich an eine
    MUI-Notification gehaengt wurde (das braucht einen Hook - bewusst
    nicht mit erzeugt, das waere ein eigenes, groesseres Feature; die
    Stubs sind ein vorbenannter Ausgangspunkt, den man selbst verdrahtet).
- **tests/test_loadsave.cpp** - Lade/Speicher-Regressionstest (doppelter
  Round-Trip gegen alle 15 echten Beispieldateien aus `BuilderSave/`):
  ALLE CHECKS BESTANDEN.
- **tests/test_codegen.cpp** - Codegenerator-Regressionstest (strukturelle
  Pruefung: Klammerbalance, BuildApplication() vorhanden, ein
  Build*Window() pro Fenster, die generierte `.c` inkludiert ihre eigene
  generierte `.h`/`_gadgets.h`, `main()`/Event-Loop/Quit-Handling in
  `_main.c` vorhanden, mindestens ein Fenster wird geoeffnet) gegen alle
  15 echten Beispieldateien: ALLE CHECKS BESTANDEN.
  - **Bugfix (Nutzer-Feedback: `m68k-amigaos-gcc` bricht mit `fatal
    error: gui.h: No such file or directory` ab):** `generateSource()`
    schrieb bisher unconditional `#include "gui.h"`, unabhaengig vom
    tatsaechlichen Basisnamen, den `generate()` fuer die `.h`/`.c`-Datei
    verwendet (z.B. `firstTest.h`/`firstTest.c`) - jede generierte `.c`
    ausser einer mit Projektname "gui" zeigte damit auf eine
    nichtexistente Datei. Das Original-Tool (`GenCodeC.c`,
    `fprintf(file, "\n#include \"%s\"\n\n", FilePart(HeaderFile))`)
    inkludiert dagegen immer den tatsaechlichen Header-Dateinamen - jetzt
    genauso portiert: `generateSource()` nimmt `baseName` als Parameter
    und schreibt `#include "<baseName>.h"`. Gegen alle 15 Beispiele
    verifiziert (jede generierte `.c` inkludiert jetzt ihre eigene `.h`).
  - **Bugfix (Nutzer-Feedback: echter `m68k-amigaos-gcc`-Build von
    generiertem Code mit Button bricht ab mit `error: 'ButtonObject'
    undeclared`, gefolgt von einer Kaskade scheinbar unzusammenhaengender
    Folgefehler):** `emitObject()` erzeugte fuer `ObjType::Button` bisher
    `ButtonObject, ButtonFrame, ...` - das neuere `ButtonObject`-Convenience-
    Makro fehlt aber in mindestens einem real verwendeten amiga-gcc-NDK
    (bebbo's Cross-Compiler-Bundle) komplett; da `ButtonObject` normalerweise
    das oeffnende `MUI_NewObject(` beisteuert, das eigentlich das
    schliessende `)` aus dem `End`-Makro weiter unten balanciert, riss ein
    fehlendes `ButtonObject` die Klammerbalance der ganzen umgebenden
    Funktion auf - daher die vielen Folgefehler (`expected ';' before ')'`,
    scheinbar unabhaengige `undeclared`-Fehler spaeter in derselben Datei
    usw.), alle mit derselben Ursache. Jetzt stattdessen `TextObject` +
    `ButtonFrame` + `MUIA_Background, MUII_ButtonBack` +
    `MUIA_InputMode, MUIV_InputMode_RelVerify` - die klassische,
    ueberall vorhandene Art, einen MUI-Button zu bauen (funktional
    identisch zu dem, was `ButtonObject` selbst expandiert, aber ohne
    Abhaengigkeit von dem neueren Convenience-Makro). `TextObject` wird
    bereits fuer den Text-Objekttyp verwendet und war im selben Build
    bereits bestaetigt fehlerfrei. Gegen alle 15 Beispiele (inkl. `Small.MUIB`
    mit seinen zwei Buttons) neu generiert und strukturell verifiziert.
    Die vom Nutzer ebenfalls gemeldeten `-Wpointer-sign`-Warnungen
    ("pointer targets ... differ in signedness") sind dagegen KEIN Bug in
    diesem Generator - sie kommen daher, dass MUI's `CONST_STRPTR`
    `const unsigned char *` ist, waehrend C-Stringliterale `char *` sind;
    das steckt in der Definition der `WindowObject`/`GroupObject`/...-Makros
    selbst (die den Klassennamen-String intern einbetten), nicht in vom
    Generator geschriebenem Code - praktisch jedes reale MUI-C-Programm mit
    diesem gcc zeigt dieselben (harmlosen) Warnungen.
  - **Bugfix (derselbe echte Build, naechster Fehler nach dem Button-Fix:
    `error: 'MUIA_Application_MenuStrip' undeclared`):** Tippfehler in
    `generateSource()` - das echte MUI-Attribut heisst
    `MUIA_Application_Menustrip` (ein Wort "Menustrip", passend zur
    Schreibweise von `MenustripObject`/`MUIA_Window_Menustrip`), nicht
    camelCase "MenuStrip"; verifiziert gegen das Original-Tool's eigenen,
    echten Quellcode (`builder.c:940`: `MUIA_Application_Menustrip,
    AppMenu = MenustripObject, End,`). Anlass genommen, systematisch JEDES
    vom Generator geschriebene `MUIA_`/`MUIM_`/`MUIV_`-Symbol (ueber 80
    eindeutige) gegen das Original-Tool's Quellbaum gegengeprueft - bis
    auf drei plausibel erklaerbare Luecken (Filelist-/Numeric-Klassen-
    Attribute, die das Original-Tool fuer seine EIGENE Oberflaeche
    schlicht nie selbst braucht, aber real und korrekt sind) war alles
    andere bereits korrekt. Gegen alle 15 Beispiele neu generiert und
    verifiziert.
  - **Bugfix (Nutzer-Feedback: `Characters.MUIB` mit `vbcc`/NDK3.2
    kompiliert, Datentyp `IPTR` fehlt in diesem NDK -
    `unknown identifier <IPTR>` in Zeile mit
    `MUIA_Cycle_Entries, (IPTR)CY_sexEntries` usw.):** `Characters.MUIB`
    nutzt Cycle/Register/Radio-Objekte, deren Entries-Arrays der
    Generator mit `(IPTR)`-Casts referenziert - das aeltere NDK-3.2-
    Include_H-Set, das vbcc hier einbindet, definiert `IPTR` (anders als
    modernere NDKs/der `m68k-amigaos-gcc`-Toolchain) gar nicht. Drei
    Aenderungen auf einmal umgesetzt, alle vom Nutzer explizit
    angefordert: 1) `generateHeader()` verankert jetzt unconditional
    `typedef unsigned long IPTR;` direkt nach `#include <exec/types.h>`
    in der generierten `.h`-Datei - ein einziger Ort, der automatisch in
    jede generierte `.c`/`_main.c`-Datei einfliesst, die diesen Header
    inkludiert; 2) `<stdio.h>` und `<stdlib.h>` stehen jetzt direkt (nicht
    nur transitiv ueber eine andere generierte Datei) im gemeinsamen
    C-Prelude (`kCHeaderPrelude`), zusaetzlich `<stdlib.h>` auch im
    Gadgets-Header; 3) `BOOL myDebug = TRUE;` in `<baseName>_main.c`
    steht jetzt direkt hinter dem letzten `#include`, vor jeder anderen
    Deklaration (vorher stand `struct Library *MUIMasterBase;` davor).
    Gegen alle 15 Beispiele verifiziert (inkl. neuer CHECKs fuer
    IPTR-Position, stdio/stdlib-Includes und myDebug-Platzierung); die
    generierte `Characters.c`/`Characters_main.c` manuell inspiziert und
    bestaetigt, dass genau die im Fehlerlog gemeldeten Zeilen jetzt vor
    einer vollstaendigen `IPTR`-Definition stehen.
  - **Bugfix (derselbe echte `Characters.MUIB`-Build, naechster Fehler
    nach dem IPTR-Fix: `error: unknown identifier <CheckMarkObject>` plus
    dieselbe Art Klammer-Kaskade wie beim frueheren `ButtonObject`-Bug):**
    `emitObject()` schrieb fuer `ObjType::Check` bislang eine erfundene
    `CheckMarkObject`-Convenience-Makro - anders als `CycleObject`/
    `RadioObject`/etc. direkt daneben (das sind echte, lang etablierte
    MUI-Klassen-Wrapper-Makros) hat MUI gar keine eigene "Check"-Klasse;
    eine Checkbox ist schlicht ein speziell konfiguriertes Image-Objekt
    (bestaetigt sowohl durch allgemeines MUI-SDK-Wissen als auch durch
    das Original-Tool's eigenen `check.c`-Dialogcode, der genau dafuer
    das echte `CheckMark()`-Makro aufruft). Fix: `ImageObject` mit den
    Attributen, auf die `CheckMark()` selbst expandiert
    (`MUIA_Frame, MUIV_Frame_ImageButton` /
    `MUIA_InputMode, MUIV_InputMode_Toggle` /
    `MUIA_Image_Spec, MUII_CheckMark` / `MUIA_Image_FreeVert, TRUE` /
    `MUIA_Background, MUII_ButtonBack` / `MUIA_Selected, ...`) - bewusst
    nicht das `CheckMark()`-Makro selbst benutzt, um nicht von dessen
    Vorhandensein in einem aelteren/alternativen NDK abhaengig zu sein
    (derselbe Grundsatz wie beim Button-Fix). Anlass genommen, alle ~19
    vom Generator geschriebenen `XxxObject`-Klassen-Wrapper-Makros gegen
    echtes MUI-SDK-Wissen gegengeprueft (dieser Bugtyp fiel durch die
    fruehere `MUIA_`/`MUIM_`/`MUIV_`-Attribut-Audit durch, da er
    Klassennamen, nicht Attribute betrifft) - `CycleObject`/`RadioObject`
    ausserdem durch denselben Fehlerlog bereits als real bestaetigt (die
    Register-/Cycle-/Radio-Widgets in `Characters.MUIB` kompilierten bis
    zur Check-Zeile durch); keine weiteren erfundenen Makros gefunden.
    Gegen alle 15 Beispiele verifiziert, generierte `Characters.c` manuell
    inspiziert - alle vier Checkmark-Gadgets (`CH_cloak`, `CH_shield`,
    `CH_gloves`, `CH_helmet`) bauen jetzt korrekt.
  - **Bugfixes (Nutzer-Feedback: `Characters.MUIB` compiliert und startet,
    aber Fenster nicht skalierbar, keine Debug-Ausgaben bei Klick auf
    Chooser/String-Gadget/Radio-Auswahl, Fenster laesst sich nicht ueber
    das Close-Gadget schliessen):** drei getrennte, echte Bugs gefunden
    und behoben:
    1) **Register-Gruppen bauten die falsche MUI-Klasse.** Eine
       registermode-Group (Seiten mit Tab-Titeln, z.B. `GR_Register` mit
       Race/Class/Armor/Level) wurde als `GroupObject` mit dem Attribut
       `MUIA_Register_Titles` gebaut - dieses Attribut gehoert aber zur
       echten, eigenstaendigen `MUIC_Register`-Klasse, nicht zu `Group`
       (bestaetigt ueber MUIBuilder's eigene einkompilierte Symboltabelle
       in `include/libraries/muibuilder.h`, die `MUIA_Register_Titles`
       explizit unter `MUIC_Register` fuehrt, getrennt von `MUIC_Group`s
       eigenen `MUIA_Group_*`-Attributen direkt daneben). Eine Basis-Group
       ignoriert ein unbekanntes Attribut stillschweigend (kein Compiler-
       oder Laufzeitfehler) - sichtbar wurde der Bug nur dadurch, dass nie
       ein Tab-Streifen erschien und ausschliesslich die zuerst hinzugefuegte
       Seite (hier: Race) je erreichbar war, exakt das vom Nutzer
       beschriebene Verhalten. Fix: `emitObject()` baut eine registermode-
       Group jetzt als `RegisterObject`. Betrifft nicht nur `Characters.MUIB`
       - beim Gegenpruefen fanden sich drei weitere reale Beispielprojekte
       mit demselben Bug (`MUI-Arc.MUIB`, `MUIB-Demo.MUIB`,
       `TextFieldPrefs.MUIB`), alle jetzt korrekt.
    2) **Kein Fenster liess sich ueber das Close-Gadget schliessen.**
       `MUIA_Window_CloseRequest` markiert nur die Schliessanfrage - sie
       schliesst nichts von selbst. Jedes echte MUI-Programm braucht dafuer
       eine eigene App-Ebene-Verdrahtung; die fehlte komplett. Fix: jedes
       generierte Fenster bekommt jetzt automatisch
       `DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, MUIV_Notify_Application, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);`
       (uebersetzt jede Schliessanfrage in ein Beenden der Applikation -
       `MUIV_Notify_Application` loest "die laufende Applikation" automatisch
       auf, ohne dass die Fenster-Funktion die `app`-Variable kennen muss,
       die zu diesem Zeitpunkt ohnehin noch gar nicht existiert).
    3) **Kein Gadget-Klick erzeugte je eine Debug-Ausgabe.** Die
       `*_Clicked()`-Stubs aus `generateGadgetStubs()` waren zwar vorhanden,
       aber - wie im ursprungliche Kommentar auch ehrlich dokumentiert -
       nirgendwo tatsaechlich verdrahtet ("Nothing calls these
       automatically"); das war fuer JEDES Gadget so, nicht nur die vom
       Nutzer konkret getesteten drei. Fix: fuer die Typen, bei denen die
       echte MUI-Klasse ein eindeutiges "der Nutzer hat gerade etwas
       gemacht"-Attribut hat (Button/`MUIA_Pressed,FALSE`,
       String/`MUIA_String_Acknowledge`, Cycle/`MUIA_Cycle_Active`,
       Radio/`MUIA_Radio_Active`, Check/`MUIA_Selected`,
       Slider/`MUIA_Numeric_Value` - siehe `isNotifiableGadgetType()`/
       `notifyAttrFor()`) verdrahtet `generateSource()` jetzt automatisch
       eine `DoMethod(..., MUIV_Notify_Application, 2, MUIM_Application_ReturnID, <id>)`-
       Notification pro Gadget, mit einer festen, deterministisch aus
       demselben Projekt neu berechneten ID (`assignGadgetNotifyIds()`);
       `generateMain()`s Event-Loop-Switch bekommt automatisch ein
       `case <id>: <ident>_Clicked(); break;` pro verdrahtetem Gadget.
       Bewusst NICHT verdrahtet (weiterhin nur der pure Stub, wie zuvor):
       ListView, DirList, Gauge, Scale, Prop, ColorField, PopAsl, PopObject,
       Image, Label, Text - deren "hat sich was geaendert"-Attribut entweder
       gar nicht existiert (Label/Text/Image) oder mehrdeutig genug ist
       (z.B. sitzt eine ListView's aktiver Eintrag eigentlich auf ihrem
       inneren List-Objekt, nicht auf dem Listview-Wrapper selbst), dass ein
       Rateversuch dasselbe Risiko wie der MUIA_Register_Titles-Bug oben
       waere.
       Vier neue Regressions-CHECKs decken das ab: registermode-Gruppen
       nutzen `RegisterObject`; jedes Fenster hat genau eine CloseRequest-
       Verdrahtung auf Quit; die IDs aus `generateSource()`s DoMethod-Aufrufen
       stimmen exakt mit den `case`-IDs aus `generateMain()` ueberein (als
       Mengenvergleich, nicht nur Anzahl); jede verdrahtete ID ist eindeutig
       und zeigt auf einen wirklich vorhandenen Stub im Gadgets-Header.
       Gegen alle 15 Beispiele verifiziert; die generierte
       `Characters.c`/`Characters_main.c` manuell inspiziert - alle 13
       notifiable Gadgets verdrahtet, IDs 1-13 auf beiden Seiten identisch.
       Keine Amiga-Laufzeitumgebung hier verfuegbar, um das tatsaechliche
       Laufzeitverhalten (Tab-Wechsel, Fenster-Resize-Bereich, Konsolen-
       Ausgabe) auf echter Hardware/Emulation selbst nachzuvollziehen -
       die drei Fixes beruhen auf Quelltext-Analyse plus etabliertem
       MUI-SDK-Wissen, nicht auf eigener Laufzeit-Verifikation. **Vom
       Nutzer auf echter Hardware/Emulation bestaetigt**: Tab-Wechsel,
       Fenster schliessen und die Debug-Ausgaben funktionieren alle wie
       erwartet.
  - **Bugfix (Nutzer-Feedback: das originale `click.MUIB`-Beispiel
    compiliert und laeuft, aber das importierte Text-Label zeigt
    "sonderbare Steuerzeichen" (`\0338\033cClick on buttons`) als
    sichtbaren Text statt als Formatierung):** per Hex-Dump der echten
    `click.MUIB`-Datei bestaetigt - das Feld speichert auf der Platte
    woertlich die Bytes `\`,`0`,`3`,`3`,`8`,`\`,`0`,`3`,`3`,`c`,... (also
    echte Backslash-Zeichen als Text, keine rohen ESC-Bytes). Das ist die
    Standard-MUIBuilder-Konvention: MUI-PreParse-Codes (Formatierung wie
    Zentrierung, Stiftfarbe, ...) werden als getippter C-Escape-Text
    gespeichert und sollen woertlich in das generierte C-String-Literal
    uebernommen werden, damit der C-COMPILER selbst `\033` beim Compilieren
    in ein echtes ESC-Byte umwandelt. `cStringLiteral()` escapte aber
    JEDEN bereits vorhandenen Backslash zusaetzlich (`\033` wurde zu
    `\\033`), wodurch ein echter C-Compiler daraus treu die LITERALEN
    Laufzeit-Zeichen `\033` statt eines ESC-Bytes erzeugte - exakt das
    gemeldete Symptom. Fix: Backslashes, die bereits im geladenen Text
    stehen, werden jetzt unveraendert durchgereicht (nur das schliessende
    `"` wird weiterhin escaped, echte eingebettete Zeilenumbrueche weiterhin
    als `\n`). Neue Regressions-CHECKs: kein generierter Text enthaelt
    einen verdoppelten Backslash; `click.MUIB`s Label reproduziert exakt
    den originalen Escape-Text. Gegen alle 15 Beispiele verifiziert.
  - **Bugfix (Nutzer-Feedback: echter `vbcc`/NDK3.2-Build von `Small.MUIB`
    - `error 82: unknown identifier <MUIA_Filelist_Drawer>`):** `Small.MUIB`
    nutzt ein DirList-Gadget; der Generator instanziierte dieses bisher als
    "best effort" ueber ein rohes `MUI_NewObject("Filelist.mcc",
    MUIA_Filelist_Drawer, ...)`, in der (wie sich jetzt zeigte falschen)
    Annahme, DirList sei eine Drittanbieter-MCC-Klasse ausserhalb der
    Basis-MUI-SDK - vgl. die weiter oben dokumentierte "drei plausibel
    erklaerbare Luecken"-Einschaetzung, die diese Annahme mangels
    Quellbaum-Beleg fuer bare Muenze nahm. Per Web-Recherche gegen MUIs
    eigene offizielle Autodocs (Dirlist.mui, gespiegelt unter
    `github.com/amiga-mui/muidev/wiki/MUI_Dirlist`) jetzt zweifelsfrei
    geklaert: `"Filelist.mcc"` existiert gar nicht, `MUIA_Filelist_Drawer`
    ist kein echtes MUI-Symbol - die tatsaechliche Klasse heisst "Dirlist"
    (Unterklasse von `List.mui`, Teil der Basis-MUI seit
    `muimaster.library` V4, also bereits in AmigaOS-3.x-Aera-MUI enthalten,
    kein Extra-Header/-Library noetig), mit dem echten Convenience-Makro
    `DirlistObject` und dem echten Attribut `MUIA_Dirlist_Directory` (V4,
    ISG, STRPTR) - genau das, was das Verzeichnisfeld des Generators
    treiben soll. Fix: `DirlistObject` + `MUIA_Dirlist_Directory` statt der
    erfundenen Filelist.mcc-Konstruktion; betraf zusaetzlich 3 weitere
    reale Beispiele (`MUI-Demo`, `MUIB-Demo`, `MUI-Arc`), alle jetzt
    korrekt. Zwei neue Regressions-CHECKs (kein erfundenes
    `Filelist.mcc`/`MUIA_Filelist_Drawer` in irgendeiner generierten Datei;
    `Small.MUIB` nutzt `DirlistObject`/`MUIA_Dirlist_Directory`). Gegen
    alle 15 Beispiele verifiziert, sauberer Build.
  - **Bugfix (Nutzer-Feedback: echter `vbcc`/NDK3.2-Build von `DVIPrint.MUIB`
    - `error 82: unknown identifier <MUII_WarpNTBadge>`):** ein
    Image-Gadget ohne eigenen Spec-String im Projekt; der Generator schrieb
    dafuer bisher einen erfundenen Platzhalter-Makronamen
    (`MUII_WarpNTBadge`), der in keiner MUI-SDK-Version existiert. Anhand
    des Original-Quellcodes (`code.c`, `TY_IMAGE`-Fall) zwei zusammenhaengende
    echte Bugs gefunden: 1) ohne eigenen Spec-String nutzt ein Image-Gadget
    eines von MUIs eingebauten Picker-Bildern (Pfeil/Diskette/Ordner/...);
    das Original-Tool schreibt dafuer den rohen numerischen Wert direkt
    (`WriteInteger(fichier, image_aux->type)`), OHNE jede symbolische
    Namensauflaesung an dieser Stelle - unser Loader liest diesen Wert
    bereits korrekt in `imgType` ein, der Generator ignorierte ihn bisher
    komplett zugunsten des erfundenen Platzhalters. Fix:
    `MUIA_Image_Spec, <imgType>,` (der bereits korrekt geladene Rohwert)
    statt eine symbolische Konstante zu raten - bewusst KEINE
    Symbol-Uebersetzungstabelle geraten, da eine falsche stillschweigend
    ein falsches, aber trotzdem compilierbares Icon erzeugen wuerde, ohne
    dass irgendein Compilerfehler das auffaengt. 2) beim ANDEREN Zweig
    (eigener Spec-String vorhanden) fehlte das `"5:"`-Praefix, das das
    Original-Tool beim Codegen immer voranstellt
    (`sprintf(buffer, "5:%s", image_aux->spec)`) - ebenfalls anhand
    `code.c` korrigiert. Zwei neue Regressions-CHECKs (kein erfundenes
    `MUII_WarpNTBadge` mehr in irgendeiner generierten Datei;
    `DVIPrint.MUIB` emittiert einen rohen numerischen
    `MUIA_Image_Spec`-Wert statt eines String-Literals). Gegen alle 15
    Beispiele verifiziert, sauberer Build.
  - **Bugfix (Nutzer-Feedback: `DVIPrint.MUIB` compiliert und laeuft, aber
    genau EIN Gadget - ein kleines Icon rechts neben dem Pfadfeld
    "Work:MUI/Demos" - liefert als einziges keine Debug-Meldung, obwohl
    alle anderen Gadgets im selben Projekt es tun):** per direktem Laden
    der echten `DVIprint.MUIB` und Objektinspektion (nicht geraten)
    bestaetigt: dieses Image-Objekt (`IM_label_0`) hat `Area.InputMode`
    gesetzt - MUIBuilders Konvention, um ein Image zu einem klickbaren
    Icon-Knopf zu machen (bestaetigt gegen das Original-Tools eigenes
    `code.c`/`CodeArea()`: `Area.InputMode` -> `MUIA_InputMode,
    MUIV_InputMode_RelVerify`). Der Generator hat dieses Flag aber
    komplett ignoriert: 1) die generische `emitArea()` emittierte nie
    `MUIA_InputMode` - fuer KEINEN Objekttyp, der ueber den generischen
    Area-Pfad laeuft, nicht nur Image; 2) Image war zusaetzlich pauschal
    von der automatischen Notification-Verdrahtung ausgeschlossen. Beides
    behoben: `emitArea()` emittiert jetzt `MUIA_InputMode`/`RelVerify`,
    wenn `Area.InputMode` gesetzt ist; `isNotifiableGadgetType()`/
    `notifyAttrFor()` wurden von einer reinen Typ-Tabelle auf
    objektabhaengige Pruefung umgestellt - ein Image zaehlt jetzt genau
    dann als "notifiable", wenn `Area.InputMode` gesetzt ist (dann wie
    ein Button ueber `MUIA_Pressed` verdrahtet), ein normales dekoratives
    Image bleibt unveraendert unverdrahtet. Betraf zusaetzlich 3 weitere
    reale Beispiele (`MUI-Demo`, `Mac2E`, `Virtual`). Zwei neue
    Regressions-CHECKs (`DVIPrint.MUIB` emittiert `MUIA_InputMode`/
    `RelVerify`; `IM_label_0` ist ueber `MUIA_Pressed` an seinen
    Debug-Stub verdrahtet). Gegen alle 15 Beispiele verifiziert, sauberer
    Build.
  - **Feature (Nutzer-Feedback: alle Gadgets in `DVIPrint.MUIB` liefern
    jetzt eine Debug-Meldung an der Shell; Wunsch nach zusaetzlichen
    Inhalts-Informationen fuer Gadgets, die selbst Inhalt tragen):** die
    `myDebug`-Stubs von `String`/`Cycle`/`Radio`/`Check`/`Slider`/
    `ListView`-Gadgets geben jetzt zusaetzlich zur bisherigen
    "wurde geklickt"-Meldung ihren aktuellen, LIVE aus dem laufenden
    Programm gelesenen Inhalt aus - ueber MUIs echtes
    `get()`/`DoMethod()`-Attributabfrage-Idiom (bewusst nicht `xget`
    verwendet, das taucht weder im Original-Quellbaum noch im bisherigen
    eigenen Code auf): `String` -> `MUIA_String_Contents` ("Mein Inhalt
    ist jetzt: ..."); `Cycle`/`Radio` -> `MUIA_Cycle_Active`/
    `MUIA_Radio_Active`, indiziert in dasselbe `CONST_STRPTR`-Entries-Array,
    das fuer die Objekterzeugung selbst schon existiert (nur wenn die
    Entries-Liste nicht leer ist); `Check` -> `MUIA_Selected`
    ("ausgewaehlt"/"nicht ausgewaehlt"); `Slider` -> `MUIA_Numeric_Value`;
    `ListView` -> `MUIA_Listview_List` gefolgt von
    `DoMethod(list, MUIM_List_GetEntry, MUIV_List_GetEntry_Active, &entry)`
    (exakte Signatur gegen die `amiga-mui/muidev`-Wiki verifiziert).
    Dabei einen echten Linkage-Bug vorab analytisch gefunden und vermieden
    (nicht erst beim Kompilieren entdeckt): die `Cycle`/`Radio`-Entries-
    Arrays waren bisher `static` (dateilokal) und wurden erst in
    `<baseName>.c` definiert, NACH dem Include von
    `<baseName>_gadgets.h` - da dessen `static inline`-Debug-Stubs aber
    separat sowohl in `<baseName>.c` als auch in `<baseName>_main.c`
    uebersetzt werden, haette ein `static` Array dort je nach
    Deklarationslage entweder gar nicht kompiliert oder (nach den
    C-Regeln fuer vorlaeufige Definitionen) in der zweiten
    Uebersetzungseinheit eine stille, leere Phantom-Kopie erzeugt statt
    der echten Daten. Fix: diese beiden Array-Typen sind jetzt echt
    extern gelinkt (kein `static` mehr - nur fuer `Cycle`/`Radio`, die
    Register-Titel-Arrays bleiben `static`, da sie nirgendwo sonst
    gelesen werden), plus eine neue `extern CONST_STRPTR ...[];`
    Vorwaertsdeklaration ganz am Anfang der generierten `.h`-Datei (vor
    jeder Nutzung, in beiden `.c`-Dateien sichtbar). Sechs neue
    Regressions-CHECKs (`String`/`ListView`-Inhaltsausgabe an
    `DVIprint.MUIB`; `Cycle`/`Check`/`Slider`-Inhaltsausgabe an
    `yak.MUIB`; die `extern`-Vorwaertsdeklaration und die echte
    non-`static`-Linkage des `Cycle`-Arrays selbst). Gegen alle 15
    Beispiele verifiziert, sauberer Build.
  - **Bugfix (Nutzer-Feedback: echter `vbcc`/NDK3.2-Build von `DVIPrint.MUIB`
    nach der Debug-Inhaltsanzeige - 10 Fehler, alle `error 82: unknown
    identifier <...>` in `DVIPrint_gadgets.h`, z.B. `<CY_label_0>`,
    `<STR_label_0>`, `<LV_label_1>`):** die neuen `get()`/`DoMethod()`-
    Inhaltszeilen (siehe voriger Punkt) referenzierten das jeweilige
    Objekt ueber `rawIdent()` - den nackten, unqualifizierten Namen -
    statt ueber `identOf()` (die `"Gui.<ident>"`-Form). Das Objekt selbst
    ist zwar bewusst als persistentes `GUIObjects`-Struct-Feld angelegt
    (genau dafuer sorgt `collectDebugContentLabels()`), aber der
    generierte Code griff trotzdem auf den nackten Namen zu, der nur
    innerhalb der Funktion existiert, die das Objekt urspruenglich baut
    (`BuildApplication()` in `<baseName>.c`) - in der komplett separaten
    `static inline`-Stub-Funktion in `<baseName>_gadgets.h` ist dieser
    nackte Name schlicht nicht deklariert, exakt das gemeldete Fehlerbild.
    Fix: der Objekt-Zugriff in allen sechs Faellen (`String`/`Cycle`/
    `Radio`/`Check`/`Slider`/`ListView`) nutzt jetzt `identOf()`
    (`"Gui.<ident>"`); nur der Name des `Cycle`/`Radio`-Entries-Arrays
    selbst (ein eigenstaendiges externes Array, kein `GUIObjects`-Feld)
    bleibt bewusst beim nackten `rawIdent()`-Namen, exakt wie schon bei
    der Objekterzeugung selbst. Bestehende Regressions-CHECKs entsprechend
    aktualisiert (erwarten jetzt `get(Gui.STR_label_0, ...)` etc.) plus
    ein neuer CHECK, der explizit sicherstellt, dass kein unqualifizierter
    Objektname mehr in einer Debug-Inhalts-`get()`-Zeile auftaucht. Gegen
    alle 15 Beispiele verifiziert, sauberer Build.
  - **Bugfix + Feature (Nutzer-Feedback: `DVIPrint.MUIB` neu generiert -
    `DirList` wirft weiterhin nie eine Meldung; eigenes Testprojekt
    "MUI-Test" gebaut - alle Gadgets ausser `DirList` und `ListView`
    (Doppelklick auf einen Eintrag) melden sich; zusaetzlich legt
    MuiBuilderQt beim Erstellen eines `ListView` keine Liste mit Inhalt
    dafuer an):** drei getrennte Befunde. 1) `DirList` (trotz `"LV"`-
    Label-Praefix - reine Namenskonvention des Original-Tools, kein
    echtes `ListView`) war schlicht nie in `isNotifiableGadgetType()`
    enthalten. Anhand MUIs offizieller Autodocs
    (`github.com/amiga-mui/muidev/wiki/MUI_Dirlist`) bestaetigt: das
    echte, dokumentierte Attribut ist `MUIA_List_Active` (geerbt von
    Dirlists echter Basisklasse `List.mui`) - jetzt fest verdrahtet, da
    unser `DirList` unverpackt erzeugt wird (nicht in eine `Listview`
    eingebettet, genau wie das Original-Tool es selbst tut). 2)
    `ListView` war ebenfalls nie verdrahtet - aber dafuer gibt es im
    Original-Tool selbst ein echtes Kontrollkaestchen ("Doppelklick",
    `list->doubleclick` in `listview.c`), das unser Loader/Saver schon
    immer korrekt aus/in `.MUIB`-Dateien gelesen/geschrieben hat, ohne
    dass es irgendwo VERWENDET wurde: weder im Codegenerator
    (`MUIA_Listview_DoubleClick` wurde nie emittiert, obwohl `code.c`s
    eigener `TY_LISTVIEW`-Fall genau das bei gesetztem Flag tut) noch im
    Property-Inspector (keine Checkbox zum Einschalten). Fix:
    `emitObject()`s `ListView`-Fall emittiert jetzt
    `MUIA_Listview_DoubleClick, TRUE` bei gesetztem Flag und verdrahtet
    es genauso wie `Button`/`Cycle`/etc; Property-Inspector hat jetzt
    eine "Doppelklick"-Checkbox fuer `ListView`, damit auch frisch in
    MuiBuilderQt gebaute Projekte das einschalten koennen. Betraf beim
    Nachgenerieren mehrere echte historische Beispiele, die dieses Flag
    laengst gesetzt hatten (`Small`, `MUI-Arc`, `MUI-Demo`, `Virtual`,
    `yak`, `GenCodeC`), ohne dass der Generator es je genutzt haette. 3)
    Zur fehlenden Listeninhalt-Erstellung: KEIN MuiBuilderQt-spezifischer
    Mangel - das Original-MUIBuilder-Tool bietet fuer `ListView`/`List`
    selbst KEINE Moeglichkeit, statischen Listeninhalt zu autoren (nur
    Format-String, optionalen FloatText-Inhalt fuer den Spezialfall
    "Floattext-Liste" sowie Hook-Funktionsnamen) - echter Listeninhalt
    wird in jedem MUI-Programm zur Laufzeit befuellt (`MUIM_List_
    InsertSingle`/`InsertList` oder die Hooks), nicht im Builder selbst;
    erwartetes, mit dem Original identisches Verhalten, kein Bug.
    Zusaetzlich `hasDebugContent()`/`emitDebugContentLines()` um
    `DirList` erweitert - meldet jetzt den ausgewaehlten Pfad ueber
    `MUIA_Dirlist_Path` (ebenfalls ueber die offiziellen Autodocs
    verifiziert: get-only, `STRPTR`). Sechs neue Regressions-CHECKs
    (`DirList`-Verdrahtung + Inhaltsmeldung an `DVIprint.MUIB`,
    `ListView`-mit-`doubleclick`-Verdrahtung an `Small.MUIB`, negative
    Kontrolle an `MUIB-Demo.MUIB`, dass eine `ListView` OHNE das Flag
    weiterhin still bleibt). Gegen alle 15 Beispiele verifiziert (1052
    CHECKs, 0 Fehler), das Nutzer-eigene "MUI-Test"-Projekt direkt
    durchgetestet, sauberer Build ohne Warnungen (GUI + Tests).
- **Bugfix: `Menu`/`SubMenu`/`MenuItem`-Codegenerierung entsprach nicht
  dem echten Original (gefunden beim Vorbereiten des interaktiven
  Menue-Editors unten, siehe dort - betrifft aber den Codegenerator
  eigenstaendig, unabhaengig von der GUI):** `emitObject()`s bisherige
  Zuordnung (`MenuObject`+`MUIA_Menu_Title` fuer `Menu`, generisches
  `Child,`-Tag fuer die Kind-Anhaengung) entsprach nicht dem echten
  Original-Tool. Anhand von `code.c`s echtem `TY_MENU`/`TY_SUBMENU`/
  `TY_MENUITEM`-Fall (Zeilen 2325-2424) korrigiert: die Menue-Wurzel
  (`Menu`) ist ein `MenustripObject` (ohne eigenen Titel) - MUIs eigenes
  `Menu.mui`/`MenuObject`/`MUIA_Menu_Title` wird vom Original-Tool
  ueberhaupt nie benutzt; `SubMenu` (sowohl fuer einen obersten
  Pulldown-Titel als auch fuer ein verschachteltes Flyout-Untermenue)
  ist immer `MenuitemObject`+`MUIA_Menuitem_Title`, mit
  `MUIA_Menu_Enabled, FALSE` wenn deaktiviert (ein eigenes, von
  `MUIA_Menuitem_Enabled` verschiedenes echtes Attribut - beide in
  `muibuilder.h`s `MUIStrings[]`-Tabelle bestaetigt); Kinder werden bei
  beiden ausschliesslich ueber `MUIA_Family_Child` angehaengt (bestaetigt
  in derselben Tabelle), nie das generische `Child`-Tag. `MenuItem`
  (Blatt) bekommt `MUIA_Menuitem_Enabled`/`Shortcut`/`Checkit`/`Checked`/
  `Toggle`, mit dem Original-Sonderfall "Name beginnt mit `BarLabel`" ->
  `MUI_MakeObject(MUIO_Menuitem, MN_BARLABEL, 0, 0, 0)` (ein
  Funktionsaufruf, kein Objekt - reiner Trennstrich). Dabei zusaetzlich
  einen zweiten, eng verwandten Bug in `generateSource()`s
  `BuildMenustrip()` (fuer `proj.appMenu`, das anwendungsweite Menue)
  gefunden: die Funktion wickelte den bereits vollstaendigen
  `MenustripObject`-Ausdruck zusaetzlich in einen hartcodierten AEUSSEREN
  `MenustripObject, Child, ..., End`-Wrapper ein - offenbar eine
  Fehlinterpretation eines Kommentars ueber MUIBuilders EIGENES internes
  Application-Objekt aus `builder.c` ("`AppMenu = MenustripObject, End,`"
  beschreibt MUIBuilders eigene GUI-Initialisierung, nicht das Muster,
  das `CodeCreate()` fuer ein Nutzerprojekt erzeugt). Laut echtem
  `TY_APPLI`-Fall erzeugt das Original das `appmenu` als EINEN
  eigenstaendigen `MenustripObject`-Ausdruck und referenziert ihn nur
  noch aus `MUIA_Application_Menustrip` - Fix: kein aeusserer Wrapper
  mehr. Da keines der 15 echten Beispiele ein nicht-leeres Menue besitzt,
  wurde diese Korrektur an einem neuen, hart programmierten synthetischen
  Testprojekt verifiziert (oberster Pulldown-Titel, verschachteltes
  Flyout, deaktivierter Eintrag, Shortcut, Checkit/Checked/Toggle,
  `BarLabel`-Trennstrich - zwoelf neue CHECKs). Gegen alle 15 Beispiele
  UND das Nutzer-eigene "MUI-Test"-Projekt erneut verifiziert (1064
  CHECKs, 0 Fehler), sauberer Build ohne Warnungen (GUI + Tests).

Beide Testsuiten laufen headless (`QT_QPA_PLATFORM=offscreen`), Build via
qmake (siehe `tests/*.pro`). Hinweis: keine dieser Testsuiten kompiliert
den generierten C-Code gegen ein echtes Amiga-SDK (dafuer fehlt hier die
Toolchain) - geprueft wird strukturelle Korrektheit (Klammerbalance,
vorhandene Funktionen/Aufrufe); der tatsaechliche `m68k-amigaos-gcc`/
`vbcc`-Build bleibt beim Nutzer.

## Bekannte, bewusste Einschraenkungen

- Notification-Argumente werden als rohe Integer-Werte aus der Datei
  uebernommen (mit Kommentar zur Handpruefung) statt als symbolische
  MUIA_*/MUIM_*-Namen - diese Namen sind ohne die fehlende Tabelle nicht
  rekonstruierbar.
- Cross-Window-Notifications auf ein nicht-persistentes (lokales) Zielobjekt
  in einer ANDEREN Fensterfunktion fuehren zu einer nicht sichtbaren
  C-Referenz (selten in den 15 Beispieldateien, bewusst nicht behoben - siehe
  CODEGEN_NOTES.md).
- Kein Katalog/.cd/Locale, kein ARexx, kein Icon-Tooltype-Parsing.

## Was noch NICHT begonnen wurde

- Die Kommandozeilen-Parameter-/Ergebnisdatei-Anbindung an AmigaED (Design
  bereits abgestimmt, aber nicht implementiert).
- GadToolsBox- und Rebuild(E-VO)-Ports - noch nicht begonnen; Quellcode
  dafuer liegt noch nicht vor.
- Echtes Undo/Redo (siehe "Bekannte Einschraenkungen der GUI" unten).

## Build (Core-Engine-Tests)

```
cd tests
qmake test_loadsave.pro && make
qmake test_codegen.pro && make
QT_QPA_PLATFORM=offscreen ./test_loadsave <Pfad-zu-BuilderSave>
QT_QPA_PLATFORM=offscreen ./test_codegen <Pfad-zu-BuilderSave> <Ausgabeverzeichnis>
```

## Interaktive GUI (gui/)

Die eigentliche interaktive Anwendung - Widget-Palette, Drag&Drop-Canvas,
Property-Inspector, Projektbaum, Datei-Menue (Neu/Oeffnen/Speichern/Code
generieren) - sitzt jetzt in `gui/` und baut direkt auf der oben
beschriebenen, bereits validierten Core-Engine auf (`gui/*.cpp` linkt
`core/muibloader.cpp`, `core/muibsaver.cpp`, `core/muicodegen.cpp` direkt
mit, wie schon `tests/*.pro`).

### Architektur

- **objectfactory.h/.cpp** - Default-Werte fuer neu angelegte Objekte
  (per Drag&Drop aus der Palette), die feste Liste der 20 in der Palette
  angebotenen Typen, und `typeHasAreaAttrs()`/`displayNameForType()` als
  von mehreren GUI-Klassen geteilte Hilfsfunktionen.
- **objecttreeutil.h/.cpp** - Baum-Mutationshelfer (Kind an eine Group
  anhaengen, Objekt aus dem Projekt entfernen, alle Labels einsammeln
  fuer eindeutige Auto-Benennung).
- **objectbox.h/.cpp** (`ObjectBox : QFrame`) - EIN visueller Kasten pro
  MuibObject. Eine Group-Box legt ihre Kinder mit einem echten
  QHBoxLayout/QVBoxLayout an (gewaehlt nach `horizontal`), sodass die
  Canvas eine woertliche, rekursiv verschachtelte Darstellung des
  Objektbaums ist statt eines separaten Diagramms davon. Nur Group-Boxen
  (und, ueber eine verschachtelte Group-Box, die Window-Box) akzeptieren
  Drops.
- **canvaswidget.h/.cpp** (`CanvasWidget : QScrollArea`) - zeigt genau
  EIN Fenster als ObjectBox-Baum; `setWindow()`/`rebuild()` bauen ihn bei
  jeder Strukturaenderung komplett neu auf (fuer die hier vorkommenden
  Baumgroessen unproblematisch und die einzige Strategie, die nie mit dem
  MuibObject-Baum auseinanderlaufen kann); `setSelected()`/
  `refreshHeaders()` aktualisieren ohne Neuaufbau.
- **widgetpalette.h/.cpp** (`WidgetPalette : QListWidget`) - Qt-eigener
  Drag-Support, Payload ist der `ObjType` als int unter dem MIME-Typ
  `kMuibObjectTypeMime` (objectfactory.h).
- **objecttreeview.h/.cpp** (`ObjectTreeView : QTreeWidget`) - spiegelt
  das GANZE Projekt (alle Fenster, inkl. deren Menue-Strang, falls
  vorhanden) read-through; Menue-Eintraege sind sichtbar, aber als "[nur
  lesbar]" markiert (siehe Scope-Entscheidung unten).
- **propertyinspector.h/.cpp** (`PropertyInspector : QWidget`) -
  generisches, pro ObjType neu aufgebautes QFormLayout; committet auf
  `editingFinished()`/`toggled()` (nicht bei jedem Tastendruck), damit
  ein Edit nur EINMAL einen Refresh ausloest.
- **mainwindow.h/.cpp** - verdrahtet alles, Datei-Menue, Fenster-Umschalter
  (Toolbar-ComboBox), Del-Taste zum Loeschen des ausgewaehlten Objekts.

### Scope-Entscheidungen (siehe auch die Kommentare in objectfactory.h/objecttreeutil.h)

- Die Palette bietet nur die 20 "echten Widget"-Typen (Group + jedes
  Blatt-Steuerelement). **Window** wird ueber Datei/Neues Fenster
  angelegt, nicht per Drag&Drop. Die **Menu/SubMenu/MenuItem**-Familie
  wird ebenfalls NICHT per Drag&Drop angeboten (Menues sind kein Teil des
  Layout-Baums), ist aber seit dem Menue-Editor-Feature (siehe unten) im
  Projektbaum selbst per Rechtsklick voll editierbar - Fenster-Menues
  (`window->menu`), nicht das anwendungsweite `MuibProject::appMenu` (siehe
  "Bekannte Einschraenkungen" unten - bewusst gleicher Scope wie zuvor).
- Der Property-Inspector zeigt bewusst NICHT jedes einzelne Feld aller 24
  Typen (das "Kitchen-Sink"-Objektmodell haette dafuer eine riesige
  Flaeche noetig) - er zeigt Label + die pro Typ wirklich
  aussehen-/verhaltensbestimmenden Felder, plus die gemeinsamen
  Area-Attribute (fuer Typen, die sie tatsaechlich nutzen) und den
  Hilfe-Titel/-text. Seltener benoetigte Felder (ListView-Hook-Namen,
  DirList-Filter, PopAsl/PopObject-Hooks, Notifications selbst) sind
  eine sinnvolle Folge-Ausbaustufe - Laden/Speichern ist davon
  unabhaengig bereits jetzt verlustfrei.

### Bekannte Einschraenkungen der GUI

- Kein echtes Undo/Redo (nur der uebliche Betriebssystem-Undo in
  Texteingabefeldern selbst). Objekt loeschen (Entf-Taste) ist
  unwiderruflich.
- Notifications (die `notify`-Liste jedes Objekts) sind im Inspector
  nicht editierbar.
- Anwendungsweites Menue (`MuibProject::appMenu`, getrennt von einem
  einzelnen Fenster-Menue) wird im Projektbaum weiterhin nicht angezeigt
  und ist nicht editierbar - der Menue-Editor (siehe unten) deckt bewusst
  nur Fenster-Menues ab, exakt wie es der Nutzer gefragt hatte ("Menüs für
  Fenster"); der zugrundeliegende Mechanismus ist identisch, eine
  appMenu-Ansicht im Baum waere eine unkomplizierte Folge-Erweiterung.

### UI-Feinschliff (Nutzer-Feedback nach dem ersten GUI-Durchlauf)

- Das Eigenschaften-Dock war zu schmal/schlecht lesbar: default-Startbreite
  jetzt deutlich groesser (`resizeDocks()` in `MainWindow::setupUi()`),
  Mindestbreite gesetzt (`inspectorDock->setMinimumWidth(340)`), und jedes
  Eingabefeld im Inspector hat selbst eine Mindestbreite (200px), damit es
  auch bei nur mittlerer Dock-Breite nicht zu eng wird. Die Größenänderung
  per Ziehen an der Dock-Grenze funktionierte technisch schon vorher (Qt
  macht das automatisch), war als Ziehgriff aber nur 1px breit und daher
  kaum zu greifen - ein `QMainWindow::separator`-Stylesheet macht ihn 6px
  breit und beim Hover farblich hervorgehoben, sodass er sich jetzt wie
  ein echter Splitter anfühlt (gilt fuer JEDE Dock-Grenze, also auch
  zwischen Palette/Projektbaum und der Canvas).
- Fenster-Position, -Groesse UND die ganze Dock-/Splitter-Anordnung
  (welches Dock wo, wie breit, angedockt oder frei schwebend) werden jetzt
  ueber `QSettings` (Gruppe "UI") gespeichert - `MainWindow::
  saveWindowSettings()` in `closeEvent()`, `MainWindow::
  loadWindowSettings()` beim Start (nach den Code-Defaults, sodass ein
  frischer Start ohne gespeicherte Werte weiterhin die sinnvollen
  Vorgaben bekommt). Verifiziert: Fenster verschoben+verkleinert, beendet
  (Strg+Q), neu gestartet - erschien exakt an der vorherigen Position mit
  der vorherigen Groesse.

### Gadgets per Drag innerhalb einer Group umsortieren

Nutzer-Feedback: "die Gadgets innerhalb einer group müssen per drag
verschiebbar sein, um ihre Reihenfolge ändern zu können." Umgesetzt:

- Jede Objekt-Box, deren Objekt selbst ein direktes Kind einer Group ist
  (also jedes Geschwister-Widget), ist jetzt zugleich Drag-Quelle -
  Maus-Press + Bewegung ueber die Qt-Standard-Drag-Schwelle startet einen
  `QDrag` mit einem neuen MIME-Typ (`kMuibObjectReorderMime` in
  `objectfactory.h`), der einfach den Quell-`MuibObject*` selbst als Payload
  traegt (gueltig nur innerhalb desselben Prozesses - hier immer der Fall).
- Als Drop-Ziel gilt entweder die Group-Box selbst (Drop in ihre eigene
  leere Flaeche/Luecken - Index wird aus der Cursor-Position relativ zu den
  existierenden Kind-Boxen berechnet) oder direkt eine Geschwister-Box
  (Drop "davor"/"danach", je nachdem auf welcher Haelfte der Box - entlang
  der Group-Ausrichtung horizontal/vertikal - losgelassen wird); Letzteres
  ist der haeufigere Fall, weil Geschwister-Boxen meist die meiste Flaeche
  einnehmen.
- **Bewusst auf "innerhalb einer Gruppe" beschraenkt** (passend zur
  Nutzer-Formulierung): ein Drop wird nur akzeptiert, wenn das gezogene
  Objekt bereits ein Kind GENAU dieser Ziel-Group ist. Kein
  gruppenuebergreifendes Verschieben in dieser Ausbaustufe - manuell
  verifiziert, dass ein Drag ueber eine fremde Group (anderer Vater)
  gar nicht erst als Drop akzeptiert wird und der Baum unveraendert bleibt.
- Neue Mutations-Hilfsfunktion `moveObjectWithinGroup()` in
  `objecttreeutil.h/.cpp` (bewegt den `unique_ptr` innerhalb/zwischen
  `children`-Vektoren, korrigiert den Ziel-Index beim Verschieben ueber die
  eigene alte Position hinweg um das erwartete Off-by-one). Signalkette
  `ObjectBox::objectReordered` -> `CanvasWidget::objectReordered` ->
  `MainWindow::onCanvasObjectReordered()`, analog zum bestehenden
  Add-Objekt-Pfad.

### Letzte Projekte (Recent-Projects-Menü)

Nutzer-Feedback: "MuiBuilder soll sich die letzten 10 Projekte merken
und in einem Menü zum Abruf bereithalten, genau wie bei AmigaED."
Umgesetzt nach genau diesem Vorbild (AmigaEDs eigenes "Letzte
Dateien"/"Recent Projects"-Untermenü als Referenzimplementierung
studiert und so getreu wie MuiBuilderQts einfacheres
Einzel-Dokument-Modell - anders als AmigaEDs Multi-Tab-Modell - es
zulaesst uebernommen):

- Neuer Menuepunkt **"Letzte Projekte"** im Datei-Menue, direkt nach
  "Oeffnen..." und vor dem folgenden Trenner - ein Untermenue mit den
  zuletzt geoeffneten/gespeicherten `.MUIB`-Projekten, neuestes zuerst.
- `QStringList m_recentProjects` (intern aeltestes-zuerst, auf
  `kMaxRecentProjects = 10` begrenzt, Duplikate werden beim erneuten
  Oeffnen an die aktuelle Position verschoben statt doppelt gelistet),
  persistiert per `QSettings` unter der Gruppe `RecentProjects`
  (Key `List`) - derselbe `QSettings`-Default-Konstruktor-Mechanismus,
  der bereits fuer die Fenstergeometrie verwendet wird
  (`QCoreApplication::setOrganizationName("MB-SoftWorX")` /
  `setApplicationName("MuiBuilderQt")` in `main.cpp`).
- Beide Wege, die ein Projekt "aktuell" machen, tragen in die Liste
  ein: `loadProjectFile()` (deckt sowohl Datei/Oeffnen... als auch den
  Kommandozeilen-Pfad ab, da dies laut eigenem Doc-Kommentar der
  gemeinsame Einstiegspunkt ist) und `onSaveProject()` (deckt sowohl
  Speichern als auch Speichern unter... ab).
- Klick auf einen Eintrag (`openRecentProject()`): prueft zuerst per
  `QFile::exists()`, ob die Datei noch da ist; falls nicht, erscheint
  eine Warnung ("Die Datei existiert nicht mehr: ... Sie wird aus der
  Liste 'Letzte Projekte' entfernt.") und der Eintrag wird sofort
  entfernt (Liste + QSettings). Existiert die Datei, greift zuerst die
  bestehende `confirmDiscardUnsavedChanges()`-Sicherung (ungespeicherte
  Aenderungen werden nicht stillschweigend verworfen), erst danach wird
  ueber `loadProjectFile()` geladen.
- Leerer Zustand: ein deaktivierter Platzhalter-Eintrag
  "(keine letzten Projekte)". Ein zusaetzlicher, immer aktiver Eintrag
  "Liste leeren" leert die gesamte Liste (inkl. QSettings) und baut das
  Untermenue neu auf den Platzhalter zurueck.
- Das Untermenue wird komplett neu aufgebaut (`updateRecentProjectsMenu()`),
  sobald sich die Liste aendert - analog zum bestehenden
  "Fenster"-ComboBox-Refresh-Muster in dieser Klasse.

**Bugfix (Nutzer-Feedback: "MuiBuilder merkt sich nicht die korrekten
Pfade, wenn die Liste der letzten Projekte angelegt/erneuert wird -
das Projekt laesst sich zwar ueber die Liste laden, aber beim
Abspeichern von Code muss man erst wieder haendisch ins korrekte
Projektverzeichnis gehen"):** kein Fehler im Recent-Projects-Mechanismus
selbst (der Pfad wird korrekt gemerkt und `loadProjectFile()` laedt
ihn auch korrekt) - der eigentliche Bug sass drei Ebenen tiefer: die
Datei-Dialoge selbst (`onOpenProject()`, `onSaveProjectAs()`, vor
allem `onGenerateCode()`s "Zielverzeichnis fuer generierten Code")
wurden bisher mit einem leeren Startverzeichnis geoeffnet
(`QFileDialog::get...(this, titel, QString(), ...)`), sodass Qt sie
irgendwo (letztes global genutztes Verzeichnis/OS-Default) statt im
Verzeichnis des AKTUELL geladenen Projekts oeffnete - unabhaengig
davon, ob dieses Projekt ueber "Letzte Projekte", "Oeffnen..." oder
die Kommandozeile geladen wurde. Genau das noetigte bei jedem
"Code generieren..." zur manuellen Neuanwahl des richtigen Ordners.
Fix: neue kleine Hilfsfunktion `currentProjectDir()` (leitet
`QFileInfo(m_currentFilePath).absolutePath()` ab, leer bei einem
neuen/ungespeicherten Projekt - dann unveraendertes altes Verhalten)
als Startverzeichnis fuer alle drei Dialoge; `onSaveProjectAs()`
bekommt zusaetzlich den vollen aktuellen Dateipfad (nicht nur das
Verzeichnis) vorbelegt, wie bei "Speichern unter" ueblich. Per
kleinem Logik-Test verifiziert (`QFileInfo::absolutePath()` liefert
fuer einen Beispielpfad exakt das erwartete Projektverzeichnis),
sauberer Rebuild, keine neuen Fehler/Warnungen.

### Menue-Editor (Fenster-Menues)

Nutzer-Frage: "wie erstelle ich eigentlich mit MuiBuilderQt Menüs für
Fenster?" - Antwort war, dass Menue-Bearbeitung bisher bewusst nicht
angeboten wurde (nur read-only im Projektbaum), gefolgt von "ja, bitte
nachziehen". Umgesetzt als Rechtsklick-Kontextmenue direkt im
Projektbaum (`ObjectTreeView`), da Menue-Eintraege nie auf dem Canvas
erscheinen:

- **`ObjectTreeView`**: `setContextMenuPolicy(Qt::CustomContextMenu)` +
  neuer `MenuTreeAction`-Enum (`AddMenu`/`RemoveMenu`/`NewTopLevel`/
  `NewSubMenu`/`NewMenuItem`/`NewSeparator`/`MoveUp`/`MoveDown`/`Delete`)
  + neues Signal `menuTreeActionRequested(action, target)` - die View
  baut nur das zum angeklickten Knotentyp passende `QMenu` auf
  (Fenster ohne Menue -> "Menue hinzufuegen"; Menue-Wurzel -> "Neuer
  Menuepunkt (oberste Ebene)"/"Menue entfernen"; `SubMenu` -> "Neuer
  Menuepunkt"/"Neues Untermenue"/"Neuer Trennstrich"/"Nach oben"/"Nach
  unten"/"Loeschen"; `MenuItem` -> "Nach oben"/"Nach unten"/"Loeschen")
  und meldet nur die Auswahl - die eigentliche Mutation macht `MainWindow`.
- **Neue Hilfsfunktionen in `objecttreeutil.h/.cpp`** (ergaenzen die
  bestehenden Group-Baum-Helfer um einen zweiten Satz fuer die
  Menue-Familie): `addMenuToWindow()`/`removeMenuFromWindow()`/
  `appendChildToMenu()`/`moveMenuChildUpDown()`/`removeMenuChild()`.
  Anders als bei den Group-Helfern ist dafuer NIE eine Projekt-weite
  Suche noetig - der Loader haengt bereits jedes Menue-Objekt (auch die
  Wurzel, an ihr Fenster) korrekt per `father` ein, ein Zielobjekt
  allein reicht also immer aus, um seine eigene Liste zu finden.
- **`objectfactory.cpp`**: `createDefaultObject()` unterstuetzt jetzt
  auch `Menu`/`SubMenu`/`MenuItem` (vorher nur in der Palette
  angebotene Typen) mit echten Default-Werten (`menu_enable = TRUE`,
  laut Original-`InitMenu()`); die automatische Label-Vergabe nutzt wie
  jeder andere Typ in diesem Port `displayNameForType()` als Basis
  (`"Menu"`/`"SubMenu"`/`"MenuItem"`) - bewusst NICHT das
  Original-Tool-eigene `"MN_label_N"`-Schema, fuer Konsistenz mit jedem
  anderen frisch angelegten Objekt hier.
- **`propertyinspector.cpp`**: `SubMenu` zeigt jetzt Titel + Aktiviert;
  `MenuItem` zusaetzlich Shortcut (1-Zeichen-Feld), Checkit, Checked,
  Toggle-Modus; ein `MenuItem` mit `BarLabel`-Namen (Trennstrich) bekommt
  ein reduziertes, reines Hinweis-Formular (keine der anderen Felder
  treffen auf einen Trennstrich zu). Die Menue-Wurzel selbst zeigt nur
  einen Hinweis (sie hat kein eigenes Titel/Aktiviert-Attribut in der
  echten MUI-Abbildung).
- **`MainWindow::onMenuTreeActionRequested()`**: fuehrt die Mutation aus
  und baut Projektbaum + Inspector direkt neu auf - bewusst OHNE
  `m_canvas->rebuild()`/`setSelected()` oder das an die
  Canvas-Selektion gekoppelte `refreshTreeAndKeepSelection()`
  anzufassen, da ein Fenster-Menue nie Teil des vom Canvas gezeigten
  Group-Baums ist (`window->menu` und `window->root` sind komplett
  getrennte Teilbaeume).
- **Voraussetzung erst gefunden, dann behoben:** beim Vorbereiten dieses
  Features stellte sich heraus, dass die bisherige Codegenerierung fuer
  `Menu`/`SubMenu`/`MenuItem` selbst nicht dem echten Original entsprach
  (siehe eigener Bugfix-Eintrag oben in "Was fertig und validiert ist")
  - erst behoben, dann der Editor darauf aufgebaut, damit neu erstellte
  Menues von Anfang an korrekten Code erzeugen.
- Bewusst NICHT umgesetzt (siehe "Bekannte Einschraenkungen" oben):
  das anwendungsweite `MuibProject::appMenu` bleibt weiterhin nicht im
  Baum sichtbar/editierbar - nur Fenster-Menues, exakt die Nutzerfrage.

Verifiziert: zwoelf neue Regressions-CHECKs am synthetischen
Menue-Testprojekt (siehe oben), gegen alle 15 echten Beispiele UND das
Nutzer-eigene "MUI-Test"-Projekt erneut durchlaufen (1064 CHECKs, 0
Fehler), generierter Code fuer das synthetische Projekt manuell
inspiziert (korrekte `MenustripObject`/`MUIA_Family_Child`-Verschachtelung,
Shortcut, `MUIA_Menu_Enabled`, Checkit/Checked/Toggle,
`BarLabel`-Trennstrich als `MUI_MakeObject()`-Aufruf). Sauberer
qmake6+make-Build ohne Warnungen (GUI + Tests). Eine GUI-Verifikation per
Xvfb+xdotool war in diesem Container-Stand nicht moeglich (kein
Window-Manager installiert, `xdotool windowactivate`/`windowfocus`
schlagen fehl) - die Verifikation stuetzt sich daher auf Codegen-Tests
plus sorgfaeltige Code-Review der Signal-/Mutations-Verdrahtung.

### AboutBox-Creator (Dialog-Creator)

Nutzer-Auftrag (mit Screenshots einer eigenen "Über Aboutbox"-Dialogbox
und einem C-Codebeispiel): "Bitte füge ins filemenu einen neuen
Menupunkt 'Dialog-Creator' mit einem Untermenü 'AboutBox' ein. Er soll
den AboutBox-creator triggern, den du noch erstellen und einbinden
musst. Dieser AboutBox-creator soll es dem user ermöglichen, Bild und
Text für eine AboutBox selbst festzulegen. Außerdem soll, falls bereits
ein Menü vorhanden ist, der user bestimmen können, mit welchem
Menüpunkt die neue AboutBox verbunden werden soll. zu guter letzt muss
der Code-Generator dann noch das öffnen der AboutBox über den
benutzerdefinierten menüpunkt triggern."

Das im pasted C-Beispiel verwendete `MUIM_Application_About` (Aboutmui,
Amiga-eingebaut) hat kein Bild-Attribut - passt also nicht zu "Bild ...
selbst festlegen" und nicht zu den Screenshots. Deshalb (auf Nutzer-
Rueckfrage per `AskUserQuestion`, dann nach Zusendung der echten MUI
5.0 SDK-Doku als `MUIDocs.zip`) stattdessen die echte, maechtigere
**Aboutbox.mcc**-Klasse verifiziert und verwendet - Teil von MUI 5.0
selbst ("AboutBox for AmigaOS", kein separates Drittanbieter-Download
mehr), verifiziert gegen `include/mui/Aboutbox_mcc.h` und
`Examples/Aboutbox.c` aus der vom Nutzer bereitgestellten SDK-Doku:

- **`core/muibobject.h`**: neuer `ObjType::AboutBox` (Wert `1000`,
  bewusst weit ausserhalb des echten `TY_*`-Bereichs 0-24, damit er nie
  mit einem echten Typ kollidiert) + neue Felder auf `MuibObject`
  (`aboutCredits`/`aboutBuild`/`aboutLogoFile`/`aboutUrl`/
  `aboutUrlText` - bilden 1:1 die echten `MUIA_Aboutbox_*`-Attribute ab;
  `aboutLinkedMenuItem` ist KEIN echtes MUI-Konzept, sondern das
  `label` des `MenuItem`s, das beim Anklicken diese AboutBox oeffnen
  soll). `MuibProject` bekommt ein neues `aboutBox`-Feld (hoechstens
  eine AboutBox pro Projekt).
- **Persistenz bewusst NICHT im echten `.MUIB`-Format**: das
  Original-Dateiformat/-Tool kennt dieses Konzept ueberhaupt nicht -
  `MuibLoader`/`MuibSaver` lesen/schreiben `ObjType::AboutBox` nirgends.
  Stattdessen ein neues, MuiBuilderQt-eigenes JSON-Sidecar
  (`core/muibqtextras.h/.cpp`, Datei `<projekt>.MUIB.mbqtextras.json`,
  nur geschrieben wenn eine AboutBox existiert) - eine bewusste
  Trennung, damit dieses neue Feature niemals die sorgfaeltig
  portierte, byte-genaue `.MUIB`-Handhabung gefaehrdet. In
  `MainWindow::loadProjectFile()`/`onSaveProject()` direkt neben den
  bestehenden `MuibLoader`/`MuibSaver`-Aufrufen eingehaengt; ein
  fehlendes Sidecar ist normal (kein Fehler), nur eine kaputte JSON-Datei
  wird gemeldet.
- **`gui/aboutboxdialog.h/.cpp`** (neu): `AboutBoxDialog` - Formular fuer
  Credits (mehrzeilig), Build, Logo-Bild (Textfeld + "Durchsuchen..."-
  Button, uebernimmt automatisch `PROGDIR:<Dateiname>` nach dem echten
  Demo-Konventions-Vorbild aus `Aboutbox.c`), URL, URL-Text, sowie eine
  Combobox "Verknuepfter Menuepunkt" - durchsucht rekursiv jedes
  Fenster-Menue nach echten `MenuItem`-Blaettern (Trennstriche und
  Eintraege ohne Label werden uebersprungen) und zeigt sie mit
  Fenster+Untermenue-Pfad an ("(nicht verknuepft)" als erste Option).
  Existiert noch kein Menue mit Eintraegen im Projekt, erscheint
  stattdessen ein Hinweistext (genau die vom Nutzer verlangte
  Bedingung: "falls bereits ein Menü vorhanden ist"). Beim Bearbeiten
  einer bestehenden AboutBox werden alle Felder vorbefuellt
  (Edit-in-Place); ein "AboutBox entfernen"-Button (nur sichtbar, wenn
  eine existiert) loescht `project->aboutBox` wieder komplett. Ein
  frisch angelegtes Label ("AboutBox", bei Kollision "AboutBox_2" usw.)
  wird automatisch vergeben, nicht vom Nutzer editierbar - wie bei jedem
  anderen automatisch benannten Objekt in diesem Port.
- **`gui/mainwindow.cpp`**: neuer Menuepunkt "Datei > Dialog-Creator >
  AboutBox..." (genau der vom Nutzer verlangte Name/Pfad); Slot
  `onDialogCreatorAboutBox()` oeffnet den Dialog und ruft `apply()` beim
  Bestaetigen.
- **`core/muicodegen.cpp` - Codegenerierung**: `ObjType::AboutBox`
  emittiert `AboutboxObject, MUIA_Aboutbox_Credits/_Build/_LogoFile/
  _LogoFallbackMode/_URL/_URLText, End` (nur gesetzte, nicht-leere
  Felder); `generateSource()` inkludiert `<mui/Aboutbox_mcc.h>` und
  deklariert `static const char *const UsedClasses[] = {"Aboutbox.mcc",
  NULL}` nur wenn eine AboutBox existiert (echte, verifizierte
  Aboutbox.c-Konvention: `MUIA_Application_UsedClasses` muss die
  Custom-Class-Nutzung ankuendigen). Die AboutBox wird als zusaetzliches
  `SubWindow` der `ApplicationObject` angehaengt (genau wie im echten
  Demo - eine AboutBox IST ein `SubWindow`, kein eigenstaendiges
  Konstrukt) und startet geschlossen. Direkt danach zwei `DoMethod()`-
  Verdrahtungen: (1) `MUIA_Window_CloseRequest` auf sich selbst setzt
  nur `MUIA_Window_Open, FALSE` (Schliessen versteckt sie nur, echte
  Demo-Konvention); (2) falls ein verknuepfter Menuepunkt existiert UND
  im Projekt gefunden wird, dessen echtes, verifiziertes
  `MUIA_Menuitem_Trigger`-Notify (mit `MUIV_EveryTime`, exakt wie
  MUIBuilder v3s eigener interner Code das verwendet) oeffnet die
  AboutBox (`MUIA_Window_Open, TRUE`) - das ist der vom Nutzer
  verlangte letzte Schritt ("Code-Generator ... muss ... das Öffnen der
  AboutBox über den benutzerdefinierten Menüpunkt triggern").
- **Persistente Gui.-Felder ueber Funktionsgrenzen hinweg**: sowohl die
  AboutBox selbst als auch der verknuepfte MenuItem brauchen ein
  persistentes `Gui.<ident>`-Feld (nicht nur eine lokale Variable),
  weil die obige `DoMethod()`-Verdrahtung in `BuildApplication()` lebt,
  der MenuItem aber in einer ANDEREN Funktion (`Build<Fenster>Window()`)
  erzeugt wird - exakt das gleiche, bereits etablierte Muster wie bei
  Notify-Zielen/Debug-Content (`collectNotifyTargets()`/
  `collectDebugContentLabels()`); neue Funktion
  `collectAboutBoxLabels()` nach demselben Muster ergaenzt und wie die
  anderen drei in allen vier `generate*()`-Funktionen identisch in
  `ctx.persistentLabels` eingemischt.
- **Bewusst NICHT umgesetzt**: das tatsaechliche Kopieren/Bundlen des
  gewaehlten Bildes in das Ausgabeverzeichnis - nur der Dateiname/-pfad
  wandert in den generierten Code (`PROGDIR:<Dateiname>`), der Nutzer
  legt das Bild selbst neben die kompilierte Amiga-Binary, genau wie im
  echten Demo (`Examples/Aboutbox.c` selbst erwartet `PROGDIR:
  boing.png` ebenfalls nur als Pfad-String).

Verifiziert: drei neue synthetische Regressions-CHECKs in
`tests/test_codegen.cpp` (AboutBox mit verknuepftem Menuepunkt - 13
CHECKs auf Attribut-/Wiring-Ebene inkl. `Gui.`-Qualifizierung beider
beteiligter Objekte; ein Projekt ganz ohne AboutBox - bestaetigt, dass
das Feature vollstaendig opt-in ist und nichts Aboutbox-Bezogenes ohne
sie emittiert wird), gesamte Suite (alle 15 echten Beispiele + die
beiden synthetischen Menue-/AboutBox-Projekte) laeuft mit
**ALL CHECKS PASSED**. Generierter Code fuer das synthetische
AboutBox-Testprojekt manuell inspiziert (siehe Auszug oben in diesem
Abschnitt an der `DoMethod`/`SubWindow`-Verdrahtung) - korrekte
`Gui.MyAboutBox`/`Gui.MI_Info`-Qualifizierung, korrekte
`MUIA_Aboutbox_*`-Attribute, korrekte `UsedClasses`/
`MUIA_Application_UsedClasses`-Kopplung. Sauberer qmake6+make-Build
ohne Warnungen (GUI + Tests). Wie beim Menue-Editor: keine
Xvfb+xdotool-GUI-Verifikation moeglich in diesem Container-Stand -
Verifikation stuetzt sich auf Codegen-Tests plus Code-Review der
Dialog-/Mutations-Verdrahtung.

### Anwendungseigenschaften (Datei > Anwendungseigenschaften...)

Beim Testen der AboutBox meldete der Nutzer zwei Beobachtungen zum
selbst erzeugten "MUI-Test"-Projekt: 1) statt des ausgewählten PNG-Bilds
erschien zunächst das AmigaED-Standardicon (Ursache per Rückfrage
geklärt: Bilddatei lag beim Test noch nicht im PROGDIR: der kompilierten
Binary UND kein PNG-Datatype installiert - kein MuiBuilderQt-Bug, nach
Beheben beider Punkte durch den Nutzer korrekt dargestellt); 2) die
Felder "Version" und "Copyright" in der laufenden AboutBox zeigten nur
die reinen Feldbezeichnungen ohne Inhalt.

Root-Cause für (2) gefunden: Aboutbox.mcc liest Version/Copyright nicht
selbst aus eigenen Attributen (die gibt es dafür gar nicht, siehe
Aboutbox_mcc.h), sondern automatisch von der besitzenden
`ApplicationObject` - also aus `MUIA_Application_Version`/
`MUIA_Application_Copyright`. Der Codegenerator emittiert diese schon
lange korrekt (verifiziert an echten Beispielen wie Characters.MUIB:
`MUIA_Application_Version, "$VER: Characters 1.1 (xx.xx.xx)"`) - das
Problem war, dass MuiBuilderQts GUI selbst NIE eine Möglichkeit bot,
`MuibProject::title/version/copyright/author/description/base` zu
setzen. Ein aus einer alten `.MUIB`-Datei geladenes Projekt hat diese
Felder (vom Original-Tool gesetzt), aber ein in MuiBuilderQt selbst neu
angelegtes Projekt (wie "MUI-Test") hatte sie für immer leer - es gab
schlicht kein Formular dafür.

Das Original-Tool selbst hat genau so ein Formular (`builder.c`s
`NewAppli()`, per "Appli"-Knopf/`ID_APPLI` aufgerufen: ein Fenster mit
Basisname/Autor/Titel/Version/Copyright/Beschreibung als StringObjects,
exakt in dieser Reihenfolge/Cycle-Chain) - direkt danach als
**`gui/projectpropertiesdialog.h/.cpp`** portiert: neuer Menüpunkt
"Datei > Anwendungseigenschaften..." (zwischen "Aktuelles Fenster
loeschen" und "Dialog-Creator"), Formular mit genau diesen sechs Feldern
(Basisname/Autor/Titel/Version/Copyright/Beschreibung), schreibt beim
Bestätigen direkt in die gleichnamigen `MuibProject`-Felder zurück -
mit demselben Dialog-Baumuster wie `AboutBoxDialog` (Konstruktor
befüllt aus dem Projekt, `apply()` schreibt zurück, `MainWindow`-Slot
`onProjectProperties()` ruft `exec()`+`apply()`+`markDirty()` auf).

Verifiziert: sauberer qmake6+make-Build ohne Warnungen; da der
Codegenerator-Teil (Emission von `MUIA_Application_*` aus den
`MuibProject`-Feldern) bereits durch alle 15 echten Beispielprojekte
abgedeckt ist (z.B. Characters.MUIB oben), bestand die eigentliche
Lücke ausschliesslich auf der GUI-Seite - der neue Dialog folgt exakt
demselben, bereits gebauten+verifizierten Baumuster wie `AboutBoxDialog`
(Konstruktion/Vorbefuellung/`apply()`/Verdrahtung), daher keine
zusätzlichen Regressions-CHECKs in `tests/test_codegen.cpp` nötig (die
Codegenerierungsseite war nie das Problem).

### Bugfix: NM_BARLABEL-Tippfehler (Menue-Trennstriche)

Nutzer hat ein neues Testprojekt ("MUItest2", mit Menue-Trennstrich)
angelegt und den generierten Code mit dem echten m68k-amigaos-gcc
compiliert; echter Compilerfehler: `'MN_BARLABEL' undeclared` in
`BuildWindowWindow()`, an der Stelle, die einen Menue-Trennstrich
erzeugt.

Root-Cause gefunden: der Codegenerator schrieb bisher `MUI_MakeObject
(MUIO_Menuitem, MN_BARLABEL, 0, 0, 0)` - mit vertauschten Buchstaben.
Der echte, korrekte Bezeichner ist **`NM_BARLABEL`** (nicht `MN_`),
verifiziert gegen die eigene, im bereitgestellten Original-Quellbaum
tatsächlich vorhandene `MUIStrings`-artige Token-Tabelle in
`include/libraries/muibuilder.h` (dort steht exakt der String
`"NM_BARLABEL"` an der Stelle, die `code.c`s `TY_MENUITEM`-Fall für
genau diesen Trennstrich-Fall schreibt) - dieselbe Tabelle also, deren
generelles Fehlen im bereitgestellten Quellbaum früher schon
dokumentiert wurde (`CODEGEN_NOTES.md`), hier aber punktuell doch
vorhanden war und den echten Bezeichner zweifelsfrei bestätigt.
`NM_BARLABEL` selbst ist der altbekannte GadTools-NewMenu-
Trennstrich-Sentinel (`((STRPTR)~0)`), den `MUI_MakeObject(MUIO_
Menuitem, ...)` fuer denselben Zweck wiederverwendet - allgemeines
AmigaOS-NDK-Wissen (die vom Nutzer bereitgestellte MUI-5.0-SDK-Doku
selbst enthaelt keine eigene `<libraries/gadtools.h>`, daher nicht
per Dokumentation, sondern per etabliertem NDK-Fachwissen verifiziert).

Fix: Tippfehler behoben (`NM_BARLABEL` statt `MN_BARLABEL`); zusaetzlich
`#include <libraries/gadtools.h>` fest in den generierten C-Prelude
aufgenommen (das Original-Tool selbst inkludiert das nirgends explizit
und verlaesst sich offenbar auf eine transitive Verfuegbarkeit ueber
irgendeinen anderen Header - bewusst nicht genauso fragil nachgebaut,
sondern der Header explizit hinzugefuegt, damit das unabhaengig vom
jeweiligen NDK/Compiler zuverlaessig aufloest).

Verifiziert: bestehender Regressions-CHECK am synthetischen
Menue-Testprojekt aktualisiert (erwartet jetzt `NM_BARLABEL`) plus zwei
neue CHECKs (das alte, nicht compilierende `MN_BARLABEL` taucht nirgends
mehr auf; `#include <libraries/gadtools.h>` ist vorhanden), gegen alle
15 echten Beispiele UND das Nutzer-eigene `muitest2.MUIB` direkt erneut
verifiziert (**ALL CHECKS PASSED**), das Nutzer-eigene Projekt zusaetzlich
manuell neu generiert und gegen die vom Nutzer mitgeschickte fehlerhafte
Version diffed - einziger inhaltlicher Unterschied ist exakt der Fix
(`NM_BARLABEL` + der neue Include). Sauberer qmake6+make-Build ohne
Warnungen (GUI + Tests).

### Bugfix: Group ohne Rahmen/Rahmentext-Eigenschaft

Nutzer-Meldung (neuer Testlauf mit `MUItest2_2.zip`, drei Punkte -
dieser Abschnitt behandelt Punkt 1): "den Groups fehlt eine
Eigenschaft, um einen Rahmen um die gruppe zu legen und einen
Rahmentext einzugeben."

Root-Cause gefunden, in zwei Teilen:

1. **GUI-Lücke**: `gui/objectfactory.cpp`s `typeHasAreaAttrs()`
   listete `ObjType::Group` bisher unter den Typen OHNE Area-Attribute
   (Hide/Disable/Weight/Frame/Background/Rahmentitel) - der
   Property-Inspector zeigte den ganzen "Area"-Abschnitt für Groups
   also gar nicht erst an. Das widerspricht dem echten Original: dessen
   eigene `struct group1` (`builder.h`) bettet ein reales `area Area;`
   -Feld ein, genau wie praktisch jeder andere Widget-Typ, und der
   zugehörige `CodeArea()`-Aufruf in `code.c`s `TY_GROUP`-Fall übergibt
   `Frame=TRUE` UND `TitleFrame=TRUE` - eine echte MUIBuilder-Group
   unterstützt also sowohl "Rahmen" (`MUIA_Frame`) als auch
   "Rahmentext" (`MUIA_FrameTitle`). `MuiCodeGen::emitObject()`s
   eigener `ObjType::Group`-Fall ruft bereits unbedingt `emitArea()`
   auf - nur die GUI hat den Abschnitt faelschlich versteckt.
2. **Codegen-Lücke** (breiter als nur Group betroffen): `emitArea()`
   selbst emittierte `MUIA_FrameTitle` bisher **gar nicht** - fuer
   KEINEN Objekttyp, nicht nur Group. Selbst bei Typen, deren
   Property-Inspector die Zeile "Rahmentitel" laengst anzeigte (z.B.
   String, Button), ging ein dort eingegebener Wert beim Codegen
   stillschweigend verloren. Der echte Bezeichner `MUIA_FrameTitle`
   (nicht `MUIA_Frame_Title`) wurde an drei unabhaengigen Quellen
   verifiziert: der eigenen `MUIStrings`-artigen Tabelle in
   `include/libraries/muibuilder.h`, der `MB_MUIA_FrameTitle`-Token-ID
   in `muib_file.h` (die `CodeArea()` fuer exakt dieses Feld schreibt),
   und dem echten MUI-SDK-Header selbst
   (`include/libraries/mui.h`: `MUIA_FrameTitle 0x8042d1c7 /* V4 isg
   STRPTR */`).

Fix:

- `gui/objectfactory.cpp`: `ObjType::Group` aus der `false`-Liste von
  `typeHasAreaAttrs()` entfernt (faellt jetzt auf `default: return
  true`) - der Property-Inspector zeigt Groups jetzt den vollen
  "Area"-Abschnitt inkl. Frame/Rahmentitel.
- `core/muicodegen.cpp`: `emitArea()` emittiert jetzt zusaetzlich
  `MUIA_FrameTitle, "<Text>",` wenn `area.TitleFrame` nicht leer ist -
  fuer JEDEN Objekttyp mit Area-Attributen, nicht nur Group.

`MUIA_Background` bleibt bewusst unangetastet: das Original kodiert
seinen Background-Wert ueber eine eigene, nicht-triviale
Index-Umrechnung (`CodeArea()`s `i = Area->Background; if (i >=
MUII_BACKGROUND) ...`), die an einen bestimmten GUI-Chooser des
Original-Tools gebunden ist, waehrend MuiBuilderQts eigenes
`AreaAttrs::Background`-Feld schon bewusst als roher Farbwert (0-
0xFFFFFF) angelegt ist - eine andere Repraesentation. Diese Umrechnung
blind zu uebernehmen haette ein hohes Risiko, falsche statt gar keine
Background-Werte zu erzeugen; das bleibt eine separate, hier nicht
angefasste Baustelle.

Verifiziert: neuer synthetischer Regressionstest (`test_codegen.cpp`,
"synthetic Group-Frame project") prueft direkt, dass eine Group mit
gesetztem `area.Frame`/`area.TitleFrame` beide Attribute korrekt
generiert; alle 15 echten Beispiele plus alle synthetischen Tests
weiterhin **ALL CHECKS PASSED**; Nutzer-eigenes `muitest2.MUIB` (aus
`MUItest2_2.zip`) erneut regeneriert, keine unerwarteten Unterschiede.
Sauberer qmake6+make-Build ohne Warnungen (GUI + Tests).

### Bugfix: AboutBox-Creator erlaubte keine Verknüpfung mit umbenannten SubMenu-Einträgen

Nutzer-Meldung (derselbe neue Testlauf, Punkt 3): "Trotz vorhandenem
menu erlaubt aboutbox-creator nicht, die erzeugte box mit einem
menupunkt zu verbinden."

Root-Cause gefunden: `gui/aboutboxdialog.cpp`s `collectMenuItems()` -
der Helfer, der die Eintraege fuer die "Verknuepfter Menuepunkt"-
Combobox sammelt - akzeptierte bisher ausschliesslich echte
`ObjType::MenuItem`-Blaetter; ein `ObjType::SubMenu`-Knoten wurde
immer nur REKURSIV durchsucht, nie selbst als Ziel angeboten. Das
Nutzer-eigene Projekt hat seinen "About..."-Eintrag aber als
`ObjType::SubMenu` gespeichert, ueber die generische, fuer JEDEN
Objekttyp immer angezeigte "Label"-Zeile im Property-Inspector auf
"SubAbout" umbenannt (bestaetigt per generiertem Code: `SubAbout =
MenuitemObject, MUIA_Menuitem_Title, "About...", ` ohne jegliche
`MUIA_Family_Child`-Kinder) - funktional ist das aber ein reines
Blatt, exakt wie ein echtes MenuItem: `muicodegen.cpp`s eigener
`ObjType::SubMenu`-Fall in `emitObject()` erzeugt fuer einen
kinderlosen SubMenu-Knoten dieselbe `MenuitemObject`/
`MUIA_Menuitem_Title`/`End`-Form wie fuer ein echtes MenuItem - keine
`MUIA_Family_Child`-Eintraege. Die eigentliche Verknuepfungs-Logik im
Codegenerator (`identOf()`/`ctx.objectByLabel`/`ctx.persistentLabels`,
in `collectAboutBoxLabels()` bereits generisch nach Label, nicht nach
`ObjType` aufgeloest) war davon nie betroffen - nur der GUI-seitige
Sammler war zu restriktiv.

Fix: `collectMenuItems()` in `gui/aboutboxdialog.cpp` erweitert -
ein `ObjType::SubMenu`-Knoten MIT leerer `childs`-Liste (und
nicht-leerem Label) wird jetzt zusaetzlich als waehlbares Ziel
aufgenommen, statt nur als Pfad-Praefix fuer tiefere Rekursion zu
dienen. Ein `SubMenu` MIT Kindern wird weiterhin ausschliesslich
rekursiv durchsucht (kein Verhaltenswechsel fuer echte Flyout-Menues).

Verifiziert: neuer synthetischer Regressionstest (`test_codegen.cpp`,
"synthetic AboutBox/SubMenu project") baut ein Menue mit einem
kinderlosen SubMenu-Blatt nach (analog zum Nutzer-eigenen "SubAbout"),
verknuepft die AboutBox damit und prueft, dass (a) der SubMenu-Knoten
weiterhin als reines Blatt ohne verschachtelte `MUIA_Family_Child`
emittiert wird und (b) der generierte `MUIM_Notify,
MUIA_Menuitem_Trigger, MUIV_EveryTime, ...`-Aufruf korrekt auf diesen
Knoten zeigt - bestaetigt, dass die Codegen-Seite bereits vollstaendig
generisch war und der reine GUI-Fix ausreicht. Alle 15 echten
Beispiele plus alle synthetischen Tests weiterhin **ALL CHECKS
PASSED**. Da diese Aenderung ausschliesslich in der GUI-Schicht liegt
(kein `.MUIB`-Ladepfad betroffen), keine Xvfb-GUI-Verifikation moeglich
in diesem Container-Stand (siehe Begruendung oben) - Verifikation
stuetzt sich auf den neuen Codegen-Test plus sorgfaeltige Code-Review
der `collectMenuItems()`-Aenderung selbst. Sauberer qmake6+make-Build
ohne Warnungen (GUI + Tests).

### Geklärt (kein Bug): "Windows nach beiden Seiten skalieren"

Nutzer-Meldung (derselbe neue Testlauf, Punkt 2): "den Windows fehlt
eine Eigenschaft, um sie nach beiden Seiten skalieren zu können."
Konkretisiert: im echten `muitest2`-Projekt laesst sich das Fenster
nur in der Breite vergroessern, nicht in der Hoehe.

Quellenrecherche im Original (`builder.h`s `struct window1`, `code.c`,
`window.c`) zeigte zunaechst: das Original-Tool hat **kein** eigenes,
explizites "in beide Richtungen skalierbar"-Feld fuer Windows - das
einzige groessenbezogene Fenster-Flag ist `sizegadget`
(`MUIA_Window_SizeGadget`, bereits in MuiBuilderQt vorhanden), das nur
das Groessenaenderungs-Gadget selbst ein-/ausblendet, ohne irgendeine
Richtung gezielt einzuschraenken. Ein thematisch passendes, aber im
mitgelieferten SDK-Header nur als blanke Konstante ohne Beschreibung
vorhandenes `MUIA_Window_SizeRight` wird vom Original-Tool nirgends
verwendet/erzeugt - bewusst nicht blind implementiert.

Stattdessen die konkrete Beobachtung im Nutzer-eigenen `muitest2.c`
direkt analysiert - Root-Cause gefunden, KEIN MuiBuilderQt-Bug: die
Wurzel-Group des Fensters (`Window_Root`) ist horizontal mit
`MUIA_Group_SameHeight, TRUE` (zusaetzlich zu `MUIA_Group_SameWidth,
TRUE`). Darin liegen nebeneinander `GroupCheckboxes` (enthaelt nur die
Checkbox, ein `ImageObject` mit `MUIA_Frame_ImageButton` - fest
dimensioniert, nicht hoehen-wachstumsfaehig) und `GroupDirView`
(enthaelt den `DirlistObject` - fuer sich genommen frei in der Hoehe
wachstumsfaehig). Da `SameHeight` beide Spalten zwingt, IMMER exakt
gleich hoch zu sein, wird die gesamte Zeile auf die winzige
Maximalhoehe der Checkbox-Spalte eingefroren - der DirList koennte
wachsen, darf aber wegen `SameHeight` nicht. In der Breite tritt das
Problem nicht auf (`SameWidth` steht hier nicht im Konflikt mit
Wachstum), daher reagiert nur die horizontale Richtung. Das ist
Standard-MUI/BOOPSI-Layoutverhalten (automatische Min/Max-Berechnung
aus dem Objektbaum) - keine fehlende MuiBuilderQt-Eigenschaft, das
"Gleiche Hoehe"-Kontrollkaestchen fuer die Wurzel-Group steht dem
Nutzer im Property-Inspector bereits zur Verfuegung, um das selbst
anzupassen (bzw. das Layout so umzubauen, dass die wachstumsfaehige
Spalte nicht mit einer starr dimensionierten in eine SameHeight-Zeile
gezwungen wird). Kein Codefix noetig/vorgenommen.

### Bugfix: Checkbox-Beschriftung wurde nicht generiert

Nutzer-Meldung (neuer Testlauf, Punkt): "die generierte Chckbox hat
keine beschriftung, obwohl eine angegeben ist."

Root-Cause: `title`/`title_exist` (Checkbox-Beschriftungsfelder) wurden
bereits korrekt geladen und im Property-Inspector angezeigt, aber
`muicodegen.cpp`s `emitObject()` hat sie fuer `ObjType::Check` schlicht
nie ausgewertet - eine Checkbox wurde immer als nackter
`ImageObject`-Checkbox-Gadget erzeugt, ganz ohne den Original-Tool
eigenen "Label2/KeyLabel2 + Checkbox in einer 2-spaltigen Gruppe"-Wrapper
(`code.c`s `TY_CHECK`-Fall, gegen die reale `struct check1` in
`builder.h` verifiziert). Fix: `emitObject()` erkennt jetzt
`title_exist && !title.isEmpty()` fuer `Check` und baut denselben
anonymen 2-spaltigen `GroupObject` (`Child, Label2(...)`/`KeyLabel2(...)`
zuerst, dann die eigentliche Checkbox als zweites Kind) wie das
Original. Zusaetzlich setzt der Property-Inspector beim Eintippen einer
Beschriftung jetzt automatisch `title_exist`, sodass auch frisch in
MuiBuilderQt angelegte (nicht nur aus einer alten `.MUIB` geladene)
Checkboxen sofort funktionieren.

Verifiziert durch einen neuen synthetischen Test ("synthetic
Check-with-label project"): eine beschriftete und eine unbeschriftete
Checkbox im selben Projekt, geprueft wird sowohl der Wrapper (Label
zuerst, Checkbox zweites Kind, richtige Spaltenzahl) als auch, dass die
unbeschriftete Checkbox weiterhin unveraendert (kein Wrapper) erzeugt
wird. Volle Regressionssuite (15 echte BuilderSave-Beispiele + alle
synthetischen Tests): **ALLE CHECKS BESTANDEN**. Keines der 15 echten
Beispielprojekte nutzt diesen Checkbox-eigenen Beschriftungsmechanismus
tatsaechlich (sie verwenden alle separat platzierte `Label`-Objekte
stattdessen), der Fix ist also nur durch den neuen synthetischen Test
abgedeckt, nicht durch echte Projekt-Regression.

### Bugfix: Hilfetexte wurden nicht generiert

Nutzer-Meldung (neuer Testlauf, Punkt): "es werden keine Hilfetexte
generiert und angezeigt, obwohl ich welche angegeben habe."

Root-Cause: Der Loader (`readHelp()` in `muibloader.cpp`) hat
`title`/`content`/`generated` fuer Fenster, Gruppen und jedes andere
Objekt schon immer korrekt aus echten `.MUIB`-Dateien gelesen - aber
`muicodegen.cpp` hat daraus **nie** irgendetwas erzeugt: kein
`MUIA_HelpNode`, kein `MUIA_Application_HelpFile`, keine `.guide`-Datei.
Im Original-Tool ist die Hilfetext-Erzeugung ein komplett separates,
eigenstaendiges Werkzeug (`guide.c`, ein eigenes "Guide"-Fenster mit
eigenem `Modify()`/`EditNode()`-Dialog), das eine reine
AmigaGuide-Hypertext-Datenbank (`.guide`) erzeugt, auf die
`MUIA_HelpNode`/`MUIA_Application_HelpFile` zur Laufzeit verweisen -
diese Trennung war in MuiBuilderQt bislang komplett unimplementiert.

Fix, direkt gegen `code.c`s `TY_*`-Faelle bzw. `guide.c`s
`GenerateGuide()`/`GuideReference()`/`WriteHelp()` verifiziert:

- `emitArea()` (jetzt fuer `const MuibObject*` statt nur `AreaAttrs`,
  damit sie auch `label`/`help` sehen kann) emittiert `MUIA_HelpNode,
  "<label>"` fuer jedes Objekt mit `help.generated && !label.isEmpty()`
  - fuer alle Typen ausser `Check` und `Scale`, die auch im Original
    keinen `MUIA_HelpNode` bekommen (bewusst NICHT "korrigiert" - das
    Original macht das genauso asymmetrisch). Fenster bekommen ihr
    eigenes `MUIA_HelpNode` separat direkt nach `MUIA_Window_Title`
    (Fenster laufen nicht ueber `emitObject()`/`emitArea()`).
- `MuiCodeGen::generateGuide()` (neu) portiert `GenerateGuide()`/
  `GuideReference()`/`WriteHelp()` aus dem Original: ein `@NODE
  <label> "<titel>" ... @ENDNODE`-Block pro Fenster (immer, mit dem
  echten Fenstertitel), pro hilfetragender Gruppe/PopObject (mit deren
  eigenem `help.title`) und pro hilfetragendem "Blatt"-Objekt (String,
  Slider, Button, Text, Image, ListView, DirList, Gauge, Cycle, Radio,
  Rectangle, ColorField, PopAsl, PopObject - dieselbe 15er-Liste wie bei
  `MUIA_HelpNode`), inklusive der `@{ " Titel " link Label }`
  Querverweiszeilen fuer direkte Kind-Objekte (rekursiert durch
  hilfelose Gruppen hindurch, wie im Original).
- `MUIA_Application_HelpFile` wird jetzt in `BuildApplication()`
  emittiert, wenn `proj.helpfile` gesetzt ist (nur der Dateiname, kein
  Pfad - wie im Original per `FilePart()`). **Eine bewusste Abweichung
  vom Original:** ist `proj.helpfile` leer, aber das Projekt hat
  irgendwo Hilfetext, wird automatisch `<Basisname>.guide` verwendet,
  statt (wie im Original) einfach gar nichts zu emittieren - sonst
  waeren alle frisch erzeugten `MUIA_HelpNode`s ins Leere zeigende
  Referenzen, weil "Anwendungseigenschaften" bislang gar kein Feld
  dafuer hatte (siehe naechster Punkt).
- **Echte Luecke, jetzt geschlossen:** "Datei > Anwendungseigenschaften"
  hatte ueberhaupt kein Feld fuer die Hilfedatei - `proj.helpfile` wurde
  zwar treu aus alten `.MUIB`-Dateien geladen/gespeichert, aber ein in
  MuiBuilderQt neu angelegtes Projekt konnte diesen Wert nie setzen. Der
  Dialog hat jetzt ein "Hilfedatei (.guide)"-Feld dafuer.
- `generate()` schreibt die `.guide`-Datei jetzt tatsaechlich mit auf die
  Platte (gleiches Gate wie `MUIA_Application_HelpFile`), unter demselben
  Dateinamen, den die Anwendung selbst referenziert.

**Bewusste Vereinfachung gegenueber dem Original:** `guide.c`s eigenes
Gate fuer "bekommt eine Gruppe/PopObject einen `@NODE`"
(`Help.generated && Help.nb_char > 0`, ohne Label-Pruefung) weicht real
leicht von `code.c`s Gate fuer "bekommt sie ein `MUIA_HelpNode`"
(`help.generated && Label nicht leer`) ab - eine echte, im Original
vorhandene Inkonsistenz, die (selten) zu einer ins Leere zeigenden
Referenz fuehren kann. MuiBuilderQt verwendet fuer beides bewusst
dasselbe, einheitliche Kriterium, damit das bei uns nie passieren kann.

**Noch nicht umgesetzt** (bewusst zurueckgestellt, da fuer die
Nutzer-Meldung nicht noetig - der Loader liest `help.title`/`content`/
`generated` bereits treu aus bestehenden Projekten): eine GUI zum
Bearbeiten von Hilfetiteln/-inhalten neuer Objekte direkt in
MuiBuilderQt (im Original ein eigener `Modify()`/`EditNode()`-Dialog mit
externem Editor). Wer aktuell neue Hilfetexte braucht, muss sie (noch)
ueber das Original-Tool oder direkt in der `.MUIB`-Datei pflegen.

Verifiziert durch einen neuen synthetischen Test ("synthetic help
project": Fenster + hilfetragende Gruppe + hilfetragendes String-Blatt),
der sowohl die `MUIA_HelpNode`/`MUIA_Application_HelpFile`-Codegen als
auch den tatsaechlichen `.guide`-Dateiinhalt (Knoten, Titel, Inhalt,
Querverweise, Anzahl `@ENDNODE`) prueft. Volle Regressionssuite:
**ALLE CHECKS BESTANDEN**.

### Bugfix: Menue-/Notify-Verkabelung wurde fast nie korrekt uebersetzt (u.a. "Quit"-Menuepunkt reagiert nicht)

Nutzer-Meldung (neuer Testlauf, Punkt): "ich sehe in der event-Schleife
keinerlei abfrage fuer Menues. So ist es z.B. nicht moeglich, auf den
Menuepunkt 'Quit' mit done == TRUE zu reagieren."

Root-Cause-Recherche ergab einen groesseren, allgemeineren Fehler als
zunaechst vermutet: `muicodegen.cpp`s `emitNotifications()` hat fuer
**jede** in einer `.MUIB`-Datei gespeicherte Notify-Verkabelung (nicht
nur Menuepunkte - Buttons, Fenster, Slider, alles) immer nur
`MUIM_Set, <destType als rohe Zahl>, <argstring>` mit einem
"von Hand pruefen"-Kommentar erzeugt, unabhaengig davon, was die Datei
tatsaechlich fuer eine Aktion gespeichert hatte. Der Grund war ein
frueher angenommener, aber falscher Blocker: das reale
`MUIBuilder v2.3`-Quellpaket enthaelt tatsaechlich die vollstaendigen
Notify-Uebersetzungstabellen (`codenotifydefs.c`s `CACTxxx[]`/
`CEVTxxx[]`/`ArgEVTxxx[]` + `initnotify.c`s `TYxxx[]`-Klassifizierung je
Objekttyp) - nur die davon komplett getrennte `MUIStrings[]`-Tabelle
(fuers reine Objektbaum-Rendering in `GenCodeC.c`) fehlt wirklich im
Quellbaum. Verwechslung geklaert, Notify-Tabellen direkt portiert.

Konkret fuer den Fall des Nutzers: Menuepunkte, Fenster, Buttons usw.
koennen im Original auf die Anwendung selbst zielen (u.a. fuer "Quit")
- die Anwendung hat dafuer ein fest einprogrammiertes, nie vom Nutzer
aenderbares Label `"App"` (`builder.c`: `strcpy(application.label,
"App")`), das in jeder echten `.MUIB`-Datei als `targetLabel` einer
solchen Notify-Verkabelung steht. Da die Anwendung in MuiBuilderQts
Objektbaum kein eigenes `MuibObject` ist, wurde dieses Label bislang nie
aufgeloest ("target label nicht gefunden") - jede auf "Quit"/"Anwendung"
zielende Verkabelung ging schlicht ins Leere, ganz unabhaengig vom
Menue-Thema.

Fix: neue Datei `core/notifytables.h`/`.cpp` mit den echten,
Objekttyp-weise portierten Aktions-/Ereignistabellen (23 Typen, aus
`codenotifydefs.c`/`initnotify.c` transkribiert - MB_*-Token sind dabei
einfach ihr eigener, entpraefixter Klartextname, keine separate
Zahlentabelle noetig). `emitNotifications()` loest jetzt Quelle
(`srcType`) UND Ziel (`destType`) tabellenbasiert auf echte
`MUIA_*`/`MUIM_*`-Namen auf (inkl. `MUIM_Application_ReturnID`,
`MUIM_List_Clear/Jump/Redraw/Remove/Sort`, `MUIM_Popstring_Open`,
`MUIM_CallHook` je nach Aktion - nicht mehr pauschal `MUIM_Set`), und
das feste `"App"`-Ziel wird jetzt erkannt und auf `BuildApplication()`s
eigene `app`-Variable abgebildet. Fuer die vier Aktionsarten, die
Original-seitig einen zweiten, unabhaengigen Objektverweis oder eine
externe Konstanten-/Funktions-/Variablenliste referenzieren (TY_WINOBJ/
TY_FUNCTION/TY_ID/TY_VARIABLE - Daten, die unser eigenes Notify-Modell
nicht erfasst), bleibt der bisherige rohe "von Hand pruefen"-Fallback
bestehen - ehrlich als bekannte, schmalere Restluecke dokumentiert statt
geraten.

Beim Testen mit den 15 echten Beispielprojekten kam ans Licht, dass
`MUI-Demo.MUIB`/`MUIB-Demo.MUIB` tatsaechlich echte, bislang nie korrekt
uebersetzte Notify-Verkabelungen enthalten (u.a. ein Fenster, dessen
CloseRequest direkt auf "Quit" der Anwendung zeigt, und mehrere
Fenster/Button-Verkabelungen, die andere Fenster oeffnen/schliessen) -
jetzt korrekt aufgeloest, z.B. `DoMethod(Gui.WI_Turn, MUIM_Notify,
MUIA_Window_CloseRequest, TRUE, app, 2, MUIM_Application_ReturnID,
MUIV_Application_ReturnID_Quit);`. Ein bestehender Regressionstest mit
einer strikten "genau 1 Vorkommen pro Fenster"-Erwartung fuer
`MUIA_Window_CloseRequest, TRUE` musste dafuer auf "mindestens 1 pro
Fenster" gelockert werden (echte, zusaetzliche Verkabelungen sind eine
Verbesserung, keine Regression). Zusaetzlich ein neuer synthetischer
Test ("synthetic menu project", erweitert): ein "Quit"-Menuepunkt mit
genau der Notify-Form, die eine echte `.MUIB`-Datei dafuer speichert
(`targetLabel="App"`, `srcType=0`, `destType=1`), erzeugt jetzt
`DoMethod(..., MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime, app,
2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);` statt
eines toten Fallback-Kommentars. Volle Regressionssuite: **ALLE CHECKS
BESTANDEN**.

### Feature: Fensterposition einstellbar (bisher immer Bildschirmmitte)

Nutzer-Meldung: ein Fenster wird immer in der Bildschirmmitte platziert,
ohne Moeglichkeit, das zu aendern. Nachforschung im kompletten
Original-Quellbaum (v2.3): das Original-Tool hat dafuer gar kein Feld -
weder `window1` (builder.h) noch `code.c` kennen/emittieren jemals
`MUIA_Window_LeftEdge`/`MUIA_Window_TopEdge` (nur ungenutzte
`MB_MUIA_Window_LeftEdge`/`MB_MUIA_Window_TopEdge`-Token-IDs existieren
in `muib_file.h`, aber kein einziger Codepfad schreibt sie). Das
"immer zentriert" ist also kein MuiBuilderQt-Fehler, sondern eine
echte, dauerhafte Einschraenkung des Original-Tools selbst (MUIs eigenes
Standardverhalten ohne gesetzte Position) - und damit ein legitimes
neues Feature, kein Bugfix im engeren Sinne.

Umgesetzt als komplett neues, MuiBuilderQt-eigenes Feature (echte MUI
5.0-SDK-Attribute/-Werte, keine Erfindung: `MUIA_Window_LeftEdge`/
`MUIA_Window_TopEdge`, "V4 isg LONG", und die echten Sonderwerte
`MUIV_Window_LeftEdge_Moused`/`MUIV_Window_TopEdge_Moused` - bestaetigt
in `include/libraries/mui.h`):

- Neues Dropdown "Fensterposition" im Property-Inspector eines Fensters
  mit drei Modi: "Zentriert (Standard)" (Default, entspricht exakt dem
  bisherigen Verhalten - es wird gar kein Attribut emittiert, ein
  unveraendertes Projekt erzeugt byte-identischen Code wie vor diesem
  Feature), "Unter Mauszeiger" (emittiert die echten
  `MUIV_Window_LeftEdge_Moused`/`MUIV_Window_TopEdge_Moused`-Sonderwerte)
  und "Manuell (X/Y)" (zwei zusaetzliche Zahlenfelder fuer feste
  Pixel-Koordinaten, nur bei diesem Modus sichtbar).
- Da das reale `.MUIB`-Dateiformat dafuer keinerlei Feld vorsieht (wie
  oben begruendet), wird die Einstellung - genau wie schon der
  AboutBox-Creator - in einer kleinen JSON-Sidecar-Datei
  (`<projekt>.MUIB.mbqtextras.json`) gespeichert, per Fenster-Label
  zugeordnet. Ein Projekt, bei dem jedes Fenster auf "Zentriert" steht
  (der unveraenderte Standardfall), erzeugt gar keine Sidecar-Datei.
- Neue synthetische Regressionstests: alle drei Modi im generierten
  C-Code geprueft (Zentriert emittiert nichts, Unter-Mauszeiger/Manuell
  die richtigen Werte), plus ein eigener Round-Trip-Test fuer die
  JSON-Sidecar selbst (Speichern, Zuruecksetzen im Speicher, Laden,
  vergleichen - inklusive Pruefung, dass die Sidecar-Datei wieder
  verschwindet, sobald alle Fenster auf "Zentriert" zurueckgesetzt
  werden). Volle Regressionssuite (15 echte Beispiele + alle
  synthetischen Tests): **ALLE CHECKS BESTANDEN**.

### Feature: Neuer Menue-Editor-Dialog (ersetzt den Rechtsklick-Kontextmenue-Editor)

Nutzer schickte zwei Screenshots eines "Menu Setup"-Dialogs (eine flache
Baumliste Menue-Name/CommKey/Typ + unten Namens-/CommKey-/Typ-Felder +
Checkboxen Menu Bar/Check/Toggle/Checked/Disabled/Mutual Group + Add/
Delete/Modify/Move Up/Move Down/OK/Cancel) als UX-Vorlage mit der Frage
"Kannst du damit etwas anfangen?". Nach Rueckfrage (`AskUserQuestion`)
zwei Entscheidungen vom Nutzer bestaetigt: (1) der neue Dialog **ersetzt**
den bisherigen Rechtsklick-Menue-Editor (siehe Eintrag oben) komplett,
nicht als Alternative daneben; (2) "Mutual Group" soll **mit eingebaut**
werden, obwohl das eine bisher unterstuetzte Funktion voraussetzt.

- **Neue Dateien `gui/menueditordialog.h/.cpp`**: der Dialog arbeitet auf
  einer TIEFEN KOPIE (`objecttreeutil.h/.cpp`'s neue `cloneMenuTree()`)
  des Fenster-Menuebaums, damit "Abbrechen" saemtliche Aenderungen
  verwerfen kann, ohne das Original je beruehrt zu haben - erst bei "OK"
  ersetzt `replaceWindowMenu()` (ebenfalls neu in `objecttreeutil.h/.cpp`)
  den echten Baum durch die bearbeitete Kopie. Die 4 "Typ"-Werte aus der
  Vorlage (Menu/Menu Item/Sub Item/Menu Bar) werden auf unser echtes
  2-Kind-Modell abgebildet (`SubMenu` = containerfaehig, `MenuItem` =
  immer Blatt), da "Typ" im echten Modell eigentlich aus Tiefe+Kindern
  abgeleitet ist, keine eigene gespeicherte Eigenschaft. Das Typ-Kombo
  wird bewusst NUR von "Hinzufuegen" gelesen, nie von "Aendern" (ein
  bereits befuelltes `SubMenu` nachtraeglich in ein Blatt-`MenuItem`
  umzuwandeln wird nicht versucht).
- **"Mutual Group" (gegenseitiger Ausschluss)**: die echte MUI-Attribut
  `MUIA_Menuitem_Exclude` existiert zwar im echten MUI-5.0-SDK (`mui.h`,
  V8, isg LONG), aber ihre Bit-pro-Geschwister-Semantik ist in keinem der
  verfuegbaren MUI-SDK-Materialien dokumentiert (auch nicht im echten
  `Examples/Menus.c`, das fuer gegenseitigen Ausschluss ein aelteres,
  hier nicht anwendbares DoMethod/GetUData/SetUData-Idiom nutzt) - und
  dieses Projekt erfindet grundsaetzlich keine unbelegte Semantik. Daher
  bewusste Design-Entscheidung: neues Feld `excludeGroup` (int,
  `muibobject.h`, nur an einem `check_enable`-Blatt-`MenuItem`
  bedeutsam) gruppiert Geschwister unter demselben unmittelbaren
  Elternknoten; die Codegenerierung (`muicodegen.cpp`, neue Methode
  `emitMenuExcludeGroupWiring()`) verkabelt jede Gruppe von 2+
  Mitgliedern paarweise ueber ausschliesslich echte, eindeutige
  MUI-Primitiven: `DoMethod(<A>, MUIM_Notify, MUIA_Menuitem_Checked,
  TRUE, <B>, 3, MUIM_Set, MUIA_Menuitem_Checked, FALSE);` fuer jedes
  geordnete Paar (A, B) der Gruppe - ein Element ankreuzen kreuzt live
  jedes andere Gruppenmitglied ab, exakt das beobachtbare Verhalten der
  Vorlage, nur mit nachvollziehbaren statt geratenen MUI-Aufrufen.
- **Bewusst begrenzter Geltungsbereich entdeckt und dokumentiert**: beim
  Verkabeln fiel ein bereits bestehender, unabhaengiger Bug in
  `BuildMenustrip()` (baut `proj.appMenu`, das anwendungsweite Menue)
  auf - dort werden `notifyLines` zwar vorab eingesammelt, aber nie ins
  generierte Ergebnis geschrieben (betrifft ALLE Notify-Typen auf dem
  App-Menue, nicht nur dieses neue Feature). Bewusst NICHT mitbehoben
  (ausserhalb des Scopes, vom Nutzer nicht gemeldet) - die neue
  `emitMenuExcludeGroupWiring()`-Verkabelung wird daher nur fuer
  Fenster-Menues (`win->menu`) aufgerufen, wo Notify-Emission tatsaechlich
  funktioniert. Die Sidecar-Persistenz selbst bleibt trotzdem generisch
  (deckt auch `proj.appMenu`-Eintraege ab, vorwaertskompatibel fuer den
  Tag, an dem dieser Altbug separat behoben wird).
- **Persistenz**: wie `posMode`/`posX`/`posY` hat `excludeGroup` keine
  Heimat im echten `.MUIB`-Format - gespeichert in derselben
  JSON-Sidecar-Datei (`core/muibqtextras.h/.cpp`, jetzt drittes Feature
  darin), per MenuItem-Label zugeordnet, rekursiver Baumdurchlauf (Menue-
  Items koennen beliebig tief verschachtelt sein, anders als die flache
  Fensterliste). Kein Eintrag in einer Gruppe -> keine Sidecar-Datei.
- **Komplette Ablösung der alten GUI-Subsystem-Teile**: `ObjectTreeView`
  verliert Kontextmenue/`MenuTreeAction`-Enum/Signal komplett (jetzt rein
  Read-only-Browsing); `MainWindow` bekommt neuen Menuepunkt "Datei >
  Menue bearbeiten..." (Strg+M) -> `onMenuEditor()` statt der alten
  `onMenuTreeActionRequested()`; `PropertyInspector` zeigt fuer die
  gesamte Menue-Familie (Menu/SubMenu/MenuItem) nur noch einen
  Hinweistext auf den neuen Dialog (alle Felder wandern in den Dialog).
- Neue synthetische Regressionstests: Codegen-Test fuer die
  Mutual-Group-Verkabelung (Top-Level-Paar, verschachteltes SubMenu-Paar
  mit derselben Gruppennummer aber anderem Elternknoten - beide duerfen
  sich NICHT gegenseitig verkabeln -, `excludeGroup == 0` und
  `check_enable == false` bekommen keine Verkabelung, eine Gruppe mit nur
  einem Mitglied ebenfalls nicht) plus ein eigener Sidecar-Round-Trip-Test
  (inklusive eines SubMenu-verschachtelten Eintrags, um den rekursiven
  Baumdurchlauf konkret zu belegen, nicht nur einen flachen Scan).
  Volle Regressionssuite (15 echte Beispiele + alle synthetischen Tests):
  **ALLE CHECKS BESTANDEN**. Sauberer qmake6+make-Build ohne Warnungen
  (GUI + Tests).
- Wie beim Menue-Editor-Vorgaenger: eine GUI-Verifikation der eigentlichen
  Dialog-Interaktion (Add/Modify/Delete/Move Up/Down/OK/Cancel) ist in
  diesem headless Container weiterhin nicht moeglich (kein Xvfb/
  Window-Manager) - Verifikation stuetzt sich auf sorgfaeltige
  Code-Review plus die Codegen-/Sidecar-Tests.

### Bugfix: Menue-Editor verschachtelte neue Eintraege zu tief

Nutzer-Meldung (mit zwei Screenshots des tatsaechlichen Bugs): ein Menue
"File" mit Menuepunkt "About" angelegt - weitere hinzugefuegte
Menuepunkte (ein Trennstrich, "Tabs") erschienen als Untermenuepunkte
von "About" statt als Geschwister unter "File", und liessen sich auch per
"Nach oben"/"Nach unten" nicht auf die richtige Ebene verschieben.
Vorschlag des Nutzers: das Typ-Kombo sollte drei echte Eintraege haben
(Menue/Menuepunkt/Untermenuepunkt statt nur zwei), jeder mit klar
definierter Hierarchie-Position.

Root Cause: die ERSTE Version des Dialogs hatte nur zwei Typ-Werte
(Menuepunkt/Untermenue) UND `onAdd()` entschied den Elternknoten per
Heuristik ("ist das aktuell selektierte Element ein Container? -> neues
Kind davon; sonst -> neues Geschwister") statt nach einer festen Regel.
Da `loadFieldsFrom()` das Typ-Kombo bei jeder Selektion auf die ECHTE Art
des selektierten Elements zuruecksetzt, genuegte es, "File" (selbst ein
Untermenue/SubMenu) selektiert zu haben, OHNE das Kombo bewusst
zurueckzustellen, damit der naechste "Hinzufuegen"-Klick "About" versehentlich
als SubMenu statt als MenuItem anlegte - und sobald irgendein Eintrag
faelschlich zum Container wurde, landete jeder danach hinzugefuegte
Eintrag, waehrend er noch selektiert war, als sein Kind statt als sein
Geschwister. Ein sich selbst verstaerkender Bug.

Behoben genau wie vom Nutzer vorgeschlagen - `gui/menueditordialog.h/.cpp`
komplett ueberarbeitet:

- Das Typ-Kombo hat jetzt drei echte Eintraege - "Menue (oberste Ebene)",
  "Menuepunkt", "Untermenuepunkt" - und jeder hat eine EINDEUTIGE,
  tiefenbasierte Platzierungsregel, die NIE von der Art des selektierten
  Elements abhaengt: "Menue" haengt immer direkt unter die Baumwurzel;
  "Menuepunkt" haengt unter das per Elternkette gefundene Menue auf
  Tiefe 1 (klettert von der aktuellen Selektion so lange nach oben, bis
  Tiefe 1 erreicht ist - funktioniert unabhaengig davon, ob das Menue
  selbst, einer seiner Menuepunkte oder ein verschachtelter
  Untermenuepunkt selektiert ist); "Untermenuepunkt" haengt unter das per
  Elternkette gefundene Menuepunkt auf Tiefe 2. Eine neue
  `depthOf()`-Hilfsfunktion (reiner Elternketten-Zaehler) treibt sowohl
  diese Platzierung als auch die Typ-Spalte im Baum UND das
  Kombo-Reflektieren bei Selektion - alles konsistent nach TIEFE, nicht
  mehr nach `ObjType`.
- Ein per "Untermenuepunkt" adressiertes Menuepunkt, das bisher ein
  einfaches Blatt-`MenuItem` war, wird an genau dieser Stelle (und nur
  hier) automatisch zu einem `SubMenu`-Container erweitert, damit es sein
  neues Kind aufnehmen kann - unbedenklich, da ein kinderloser `SubMenu`
  exakt wie ein `MenuItem`-Blatt generiert wird (siehe `muicodegen.cpp`).
  `onDelete()` macht das spiegelbildlich rueckgaengig, sobald das letzte
  Kind eines so erweiterten Menuepunkts geloescht wird, damit es nicht
  dauerhaft intern als (jetzt leerer) Container haengen bleibt.
- Fehlt beim Hinzufuegen eines Menuepunkts/Untermenuepunkts eine
  passende Selektion, um die Zielebene eindeutig zu bestimmen, erscheint
  jetzt ein klarer Hinweisdialog statt eines stillschweigend falschen
  Ergebnisses.
- Die Typ-Spalte im Baum zeigt jetzt ebenfalls tiefenbasiert an
  ("Menuepunkt" bleibt "Menuepunkt", auch nachdem es intern zum
  `SubMenu`-Container erweitert wurde, statt ploetzlich als "Untermenue"
  zu erscheinen) - entspricht damit genau der mentalen Modellvorstellung
  des Nutzers.
- "Nach oben"/"Nach unten" (`moveMenuChildUpDown()` in
  `objecttreeutil.cpp`) brauchten selbst keine Aenderung - sie sortieren
  bereits rein innerhalb der Geschwisterliste des tatsaechlichen
  Elternknotens; sobald `onAdd()` diese Liste korrekt befuellt, sortieren
  sie automatisch auf der richtigen Ebene.

Sauberer qmake6+make-Rebuild ohne Warnungen (GUI). Volle
Codegen-Regressionssuite (15 echte Beispiele + alle synthetischen Tests,
unberuehrt von dieser reinen GUI-Aenderung) weiterhin **ALLE CHECKS
BESTANDEN**. Wie bei jeder GUI-Aenderung in diesem Projekt: eine
Xvfb-basierte Interaktionsverifikation ist in diesem Container nicht
moeglich (kein Window-Manager) - verifiziert per sorgfaeltiger
Schritt-fuer-Schritt-Code-Nachvollziehung des genau vom Nutzer
gemeldeten Szenarios (File anlegen, About als Menuepunkt darunter,
weitere Menuepunkte waehrend About selektiert hinzufuegen) gegen die
neue Platzierungslogik.

### Feature: Fenstergroesse einstellbar (Width/Height/MinWidth/MinHeight/MaxWidth/MaxHeight)

Nutzer-Meldung (mit echtem, auf m68k-amigaos-gcc compiliertem
Testprojekt `menuTest` und zwei WinUAE-Screenshots, die den Lauf auf
echtem AmigaOS 3.2/MUI belegen): Fenster bleiben initial sehr klein,
es fehlt eine Moeglichkeit, Width/Height/MinWidth/MinHeight/MaxWidth/
MaxHeight zu setzen. Wie schon bei der Fensterposition: `window1`
(builder.h) hat dafuer gar keine Felder - ein legitimes, neues
MuiBuilderQt-Feature, kein Bugfix. Gegen die echte MUI 5.0-SDK-Header
(`include/libraries/mui.h`) verifiziert: `MUIA_Window_Width`/
`MUIA_Window_Height` sind echte, fenster-eigene Attribute (V4, isg
LONG); `MUIA_MinWidth`/`MUIA_MinHeight`/`MUIA_MaxWidth`/
`MUIA_MaxHeight` sind zwar ebenfalls echt, aber GENERISCHE
Area-Attribute (gelistet neben MUIA_Background/MUIA_Frame/MUIA_Weight,
nicht unter einer "MUIA_Window_"-Ueberschrift) - sie muessen also auf
das Wurzel-Group-Objekt des Fensterinhalts, nicht auf das
WindowObject selbst. Ein "MUIA_Window_MinWidth" o.ae. existiert in der
echten SDK gar nicht.

- Sechs neue Fenster-Felder (`winWidth`, `winHeight`, `winMinWidth`,
  `winMinHeight`, `winMaxWidth`, `winMaxHeight`, 0 = "nicht gesetzt" -
  dieselbe Konvention wie beim Menue-Editors eigenem "Mutual-Exclude-
  Gruppe (0 = keine)"-Feld) in `core/muibobject.h`.
- Codegen: Width/Height landen auf dem WindowObject selbst, Min/Max auf
  dem Wurzel-Group ueber einen neuen, chirurgisch minimalen
  `rootExtrasFrom`-Parameter an `emitObject()` - injiziert Fenster-
  spezifische Extra-Attribute in genau den einen rekursiven Aufruf, der
  das Fenster-Wurzel-Group emittiert, ohne jeden anderen Aufruf dieser
  generischen Funktion zu verunreinigen.
- Persistenz ueber dieselbe JSON-Sidecar-Datei wie Fensterposition/
  AboutBox (jetzt das vierte darin gespeicherte Feature) - abwaerts-
  kompatibel, derselbe `windowPositions`-Array-Schluessel.
- Sechs neue Zahlenfelder im Property-Inspector eines Fensters
  ("Breite/Hoehe/Min. Breite/Min. Hoehe/Max. Breite/Max. Hoehe (0 =
  nicht gesetzt)").
- Neue synthetische Regressionstests: Codegen (alle sechs Felder auf 0
  emittiert nichts - byte-identisch zu vorher; gesetzte Werte landen an
  der richtigen Stelle) plus ein eigener Sidecar-Round-Trip-Test. Volle
  Regressionssuite (15 echte Beispiele + alle synthetischen Tests):
  **ALLE CHECKS BESTANDEN**.

### Feature: Menue-Editor - "Aktion" (Notify-Verkabelung fuer Quit und Fenster-Aktionen)

Nutzer-Meldung: es fehlt eine Moeglichkeit, dem Menuepunkt "Quit" eine
Aktion zuzuweisen (Programm beenden oder eine Funktion aufrufen).
Projektweite Suche bestaetigt: es gab bislang GAR KEINE GUI, die
`obj->notify` ueberhaupt erzeugt - `notifytables.h`'s eigener
Kopfkommentar dokumentiert das sogar selbst ("not needed here since we
don't (yet) have a notify-editing UI"), ein echter, bisher
unentdeckter Luecke, nicht auf Menues beschraenkt.

Umgesetzt als kleines, bewusst KURATIERTES (nicht vollstaendig
generisches) "Aktion"-Kombo im Menue-Editor-Dialog, nur fuer Blatt-
Menuepunkte, mit echten, gegen `notifytables.cpp` verifizierten
Eintraegen (direkt aus dem Original-Tool's `codenotifydefs.c`
portiert, nicht erfunden):

- "Programm beenden (Quit)" (der konkret gemeldete Fall).
- "Fenster oeffnen/schliessen/aktivieren: <Titel>" fuer jedes andere
  Fenster im Projekt (z.B. ein "Einstellungen..."-Menuepunkt).
- "Eine Funktion aufrufen" ist bewusst NICHT umgesetzt: das Original-
  Tool's `TY_FUNCTION`-Notify-Aktionen referenzieren die eigene
  Functions-Namensliste des Projekts per Zeiger - Daten, die
  `NotifyEvent` nie erfasst hat. Ein plausibel aussehender, aber nicht
  verifizierter Workaround wuerde die eigene Kernregel dieses Projekts
  verletzen - stattdessen als dokumentierte, bekannte Luecke belassen.
- Datensicherheit: ein reales, bereits geladenes `.MUIB`-Projekt kann
  Notify-Verkabelung enthalten, die dieses kleine kuratierte Kombo
  nicht abbilden kann (mehrere Events auf einem Eintrag, oder ein
  Ziel/Aktion-Paar ausserhalb der beiden obigen Faelle). Ein
  dediziertes "Unveraendert lassen"-Sentinel wird automatisch
  ausgewaehlt, wann immer `obj->notify` nicht exakt zu einer der
  echten Kombo-Optionen passt - so kann das blosse Oeffnen des Dialogs
  und Klicken auf "Aendern", ohne das Kombo anzufassen, niemals
  vorhandene, nicht abbildbare Verkabelung aus einer echten Datei
  zerstoeren.
- Container-Objekte (SubMenu) werden von diesem Feature nie beruehrt -
  `obj->notify` bleibt dort komplett unangetastet, da SubMenu selbst
  eine echte, gueltige Notify-Quelle/-Ziel im Original-Tool ist
  (`notifytables.cpp`'s `actionsSubMenu()`).

Kein neuer Codegen-Pfad noetig (nutzt die bereits getestete
`emitNotifications()`-Maschinerie weiter) - volle Regressionssuite
weiterhin **ALLE CHECKS BESTANDEN**.

### Feature: Debug-Stub-Funktionen fuer Menuepunkte (wie bei Gadgets)

Nutzer-Vorschlag (waehrend die beiden obigen Features noch in Arbeit
waren): jeder Menue- und Untermenue-Eintrag (ausser der AboutBox und
spaeter zu implementierenden weiteren Dialogboxen) sollte - genau wie
Buttons/String/Cycle/... es bereits tun (auf echter Hardware per
WinUAE-Screenshot belegt: "Ich bin BtnFuck und wurde geklickt") - eine
leere, von Hand editierbare Debug-Stub-Funktion bekommen, die im
Schema "Ich bin Menuepunkt XYZ und wurde getriggert" eine `if(myDebug)`-
gekapselte Meldung ausgibt.

Die bestehende Gadget-Stub-Maschinerie (`isStubbableGadgetType()`/
`isNotifiableGadgetType()`/`notifyAttrFor()`/`collectGadgetObjects()`/
`generateGadgetStubs()`/das Fenster-Bauschleifen-Notify-Wiring/
`generateMain()`'s Switch-Dispatch) erweitert, statt eine zweite,
parallele Struktur zu erfinden:

- `ObjType::MenuItem` ist jetzt "stubbable" - das echte Datenmodell
  kennt nur zwei Menue-Arten (`SubMenu` = Container, nie klickbar;
  `MenuItem` = immer ein Blatt, unabhaengig von der GUI-Tiefen-
  Bezeichnung "Menuepunkt"/"Untermenuepunkt") - also entspricht "jeder
  echte, auswaehlbare Menue-Eintrag" exakt "jedes beschriftete
  `ObjType::MenuItem`".
- `ObjType::MenuItem` ist unbedingt "notifiable" (kein Opt-in-Flag
  noetig, anders als bei Image/ListView) und feuert das echte
  `MUIA_Menuitem_Trigger`/`MUIV_EveryTime` (bereits beim "Aktion"-
  Feature oben gegen `notifytables.cpp`'s `eventsMenuItem()[0]`
  verifiziert) - genau dieselbe automatische DoMethod-Verkabelung +
  `generateMain()`-Dispatch, die Buttons & Co. bereits bekommen.
- Die eigene Wortwahl der Meldung ("Ich bin Menuepunkt XYZ und wurde
  getriggert" statt "...und wurde geklickt") ist bewusst - "geklickt"
  ist fuer einen Menue-Eintrag schlicht falsch (kein Mausklick im
  eigentlichen Sinn erreicht ihn - Tastenkuerzel/MUIs eigene Menue-
  Maschinerie loesen ihn genauso aus), "getriggert" passt zum echten
  MUI-Eventnamen.
- BarLabel-Trennstriche (`obj->name.startsWith("BarLabel")`, dieselbe
  Konvention wie im Menue-Editor selbst) bekommen KEINEN Stub - ein
  Trennstrich ist nie auswaehlbar und sein `label` ist nur eine interne
  Buchungs-ID, keine echte, fuer eine Meldung sinnvolle Bezeichnung.
- Der mit der AboutBox verknuepfte Menuepunkt (`aboutBox-
  >aboutLinkedMenuItem`) bekommt weder Stub noch Verkabelung noch
  Switch-Case - sein echter Job ist das Oeffnen der AboutBox, wie vom
  Nutzer selbst gefordert ("ausgenommen AboutBox"). Als einfacher,
  expliziter Check gegen den heute einzigen konkreten Fall umgesetzt
  (nicht als generischer "Dialog-verknuepft"-Mechanismus) - passend
  erweiterbar, sobald weitere Dialogboxen dazukommen.
- Eine bisher latente strukturelle Luecke aufgedeckt und sauber
  behoben: `proj.appMenu` (das anwendungsweite Menue, getrennt von
  jedem Fenster-eigenen Menue) wird in `BuildMenustrip()` gebaut, einer
  Funktion, die - anders als jedes `BuildXWindow()` - noch nie eine
  Notify-Emission-Stufe hatte (dieselbe, bereits dokumentierte
  Einschraenkung, die schon die "Mutual Group"-Verkabelung betrifft,
  siehe `muicodegen.h`'s `emitMenuExcludeGroupWiring()`-Kommentar).
  Ein `proj.appMenu`-Menuepunkt bekommt daher weiterhin einen echten,
  korrekt beschrifteten Stub (nuetzlicher, von Hand verdrahtbarer
  Ausgangspunkt), aber bewusst keine Notify-ID/Verkabelung/Switch-Case
  - alles andere waere unehrlicher generierter Code (ein Case, der
  wirkt, als waere er erreichbar, es aber nie ist). Neue eigene
  `collectWireableGadgets()`-Funktion (fenster-only) fuer die Notify-
  ID-Vergabe, waehrend `collectAllGadgets()` (weiterhin inklusive
  `proj.appMenu`) nur fuer die Stub-Textgenerierung selbst verwendet
  wird.

Neue synthetische Regressionstests: Stub-Generierung (inkl. Tiefe-2-
Eintraege, BarLabel-Ausschluss, AboutBox-Ausschluss, Container-
Ausschluss), echte DoMethod-Verkabelung mit eindeutigen IDs pro
Menuepunkt, `generateMain()`-Dispatch, sowie ein eigener Test fuer die
`proj.appMenu`-Sonderbehandlung (Stub ja, Verkabelung/Dispatch nein).
Sauberer qmake6+make-Rebuild ohne Warnungen (beide Targets). Volle
Regressionssuite (15 echte Beispiele + alle synthetischen Tests):
**ALLE CHECKS BESTANDEN**.

### Bugfix: fehlende objectName() an Docks/Toolbar (saveState()-Warnung)

Nutzer-Meldung (aus einem echten Windows/Qt-Creator-Build-Log): beim
Beenden erscheinen mehrere `QMainWindow::saveState(): 'objectName' not
set for QDockWidget/QToolBar`-Warnungen - unkritisch (die eigene
`restoreState()`-Versionspruefung dieses Projekts verwirft ohnehin
jedes gespeicherte Layout aus einer aelteren Version, siehe
`kWindowStateVersion`), aber ein echter, einfacher Fix statt nur eine
Notiz fuer spaeter. Ursache: `setupDocksAndPanels()`/
`setupMenusAndToolbar()` (`gui/mainwindow.cpp`) erzeugten alle drei
Docks (Palette/Projektbaum/Eigenschaften) und die Fenster-Umschalter-
Toolbar ohne `setObjectName()` - Qt's `saveState()`/`restoreState()`
schluesseln jedes Dock/jede Toolbar aber genau darueber, ein leerer
Name kollabiert sonst alle unbenannten Widgets auf denselben Schluessel.
Behoben durch je einen festen `setObjectName()`-Aufruf direkt nach der
Konstruktion. Sauberer qmake6+make-Rebuild ohne Warnungen; reine
GUI-Aenderung, Codegen-Regressionssuite unberuehrt.

### Feature: "running" als globale Variable + Debug-Stub-Kommentare mit dem echten Menuepunkt-Titel

Nutzer-Meldung (mit echtem `menuTest2`-Projekt): schlug vor, die
Event-Schleifen-Variable `running` in `<baseName>_main.c` global zu
machen, damit z.B. der "Quit"-Menuepunkt sie per Hand-editiertem
Debug-Stub auf `FALSE` setzen kann, um das Programm sauber zu beenden -
hat das selbst im generierten Code umgesetzt, aber "Quit" beendete das
Programm trotzdem nicht.

Beide Teile bearbeitet:

- **Das Feature selbst umgesetzt**: `running` ist jetzt (wie schon
  `myDebug`) eine echte globale Variable in `<baseName>_main.c`, mit
  `extern BOOL running;` in `<baseName>_gadgets.h` - jeder Stub kann
  jetzt einfach `running = FALSE;` setzen, um das Programm zu beenden.
  Reine Sichtbarkeits-Aenderung, keine Verhaltensaenderung der
  Event-Schleife selbst.
- **Die eigentliche Ursache des Bugs gefunden**: im hochgeladenen
  Projekt war GAR NICHT der Quit-Menuepunkt (`MenuItem_5`, echter
  Titel "Quit") editiert worden, sondern `MenuItem_2` - dessen echter
  Titel ist "Manual" (unter "Help")! Die automatisch vergebenen,
  laufnummernbasierten Standard-Labels ("MenuItem_2", "MenuItem_5" -
  Vergabereihenfolge, nicht Position/Titel) machen genau diese
  Verwechslung leicht moeglich, und selbst die eigene Debug-Meldung
  im Stub ("Ich bin Menuepunkt MenuItem_2 ...") haette nicht geholfen,
  da sie ebenfalls nur das Label, nie den echten Titel zeigt.
- **Root-Cause-Fix statt nur Diagnose**: bewusst KEIN neues
  Label-Namensschema fuer Menuepunkte eingefuehrt (dieses Projekt haelt
  Standard-Labels absichtlich fuer JEDEN Typ gleich - siehe
  `objectfactory.cpp`'s eigener Kommentar dazu, warum das ORIGINAL-Tools
  eigenes "MN_label_N"-Schema bewusst NICHT nachgebaut wurde). Stattdessen
  bekommt jeder Debug-Stub jetzt einen rein zusaetzlichen `/* "..." */`
  Kommentar direkt ueber der Funktion mit dem echten, sichtbaren Text
  (`MUIA_Menuitem_Title` bei Menuepunkten, sonst `title` - z.B. bei
  Button/String/Cycle/...), z.B. `/* "Quit" */` ueber
  `MenuItem_5_Clicked()` - null Risiko fuer bestehenden generierten
  Code, aber ab sofort auf einen Blick erkennbar, welcher Stub zu
  welchem echten Eintrag gehoert. Derselbe Kommentar erscheint auch im
  `switch`-Dispatch in `<baseName>_main.c` (z.B.
  `case 2: /* MenuItem_2 "Manual" */`).

Das hochgeladene `menuTest2`-Projekt mit dem echten, eigenen Loader neu
generiert (inkl. JSON-Sidecar) zur Verifikation: bestaetigt, dass
`/* "Quit" */` jetzt korrekt ueber `MenuItem_5_Clicked()` steht (nicht
ueber `MenuItem_2_Clicked()`, dessen Kommentar korrekt `/* "Manual" */`
zeigt) und `running` global + extern-deklariert ist. Neue synthetische
Regressionstests fuer beide Aenderungen; ein bestehender Regex-Test
(Uebereinstimmung der Notify-IDs zwischen Quellcode und `main()`) an das
neue, optionale Kommentarformat angepasst. Beide Build-Targets sauber
ohne Warnungen neu gebaut, volle Regressionssuite (15 echte Beispiele +
alle synthetischen Tests, inklusive aller 15 echten Beispiele, deren
Gadgets in aller Regel einen vom Label abweichenden Titel haben und
damit den neuen Kommentarpfad breit mitpruefen): **ALLE CHECKS
BESTANDEN**.

### Feature: Menue-Editor - "ID (intern)" umbenennbar + in der Hierarchie sichtbar

Nutzer-Meldung: es waere hilfreich, wenn man die interne ID (`label`)
eines Menuepunkts/Untermenuepunkts im Menue-Editor selbst zu einem
"sprechenden Namen" aendern koennte - z.B. ueber ein String-Widget mit
dem aktuellen Default-Wert vorbelegt (z.B. "MenuItem_5"); der
Code-Generator muesse das entsprechend verarbeiten, damit die
Programmlogik erhalten bleibt. Direkt danach als Ergaenzung: die ID solle
zusaetzlich im Hierarchiefenster hinter jedem Eintrag angezeigt und bei
Aenderung aktualisiert werden.

Beide Teile umgesetzt:

- **Neues Feld "ID (intern)"** im Menue-Editor-Dialog, direkt unter
  "Name": ein QLineEdit mit Validator (nur gueltige C-Bezeichner), bei
  Auswahl eines bestehenden Eintrags immer mit dessen aktuellem Label
  vorbelegt; bei "Hinzufuegen" zeigt es schon vorab eine Live-Vorschau
  des Labels, das ein Klick auf "Hinzufuegen" gerade vergeben wuerde
  (dieselbe `makeUniqueLabel()`-Logik wie bisher), solange der Nutzer das
  Feld nicht selbst angefasst hat - Wechsel des Typ-Combos vorher
  ueberschreibt einen bereits selbst eingetippten Wert also nicht mehr.
- **Neue Baum-Spalte "ID (intern)"** in der Hierarchie-Ansicht des
  Dialogs (4. Spalte, nach "Menue-Typ"), die bei jedem Hinzufuegen/Aendern
  automatisch mit aktualisiert wird (die Tabelle wird nach jeder Aktion
  ohnehin neu aufgebaut).
- **Validierung**: leer, ungueltige Zeichen (muss mit Buchstabe/
  Unterstrich beginnen, danach nur Buchstaben/Ziffern/Unterstriche) oder
  bereits anderswo im GESAMTEN Projekt verwendet -> Fehlermeldung, nichts
  wird angewendet (weder bei "Hinzufuegen" noch bei "Aendern" - beide
  bleiben dadurch atomar: entweder alle Feldaenderungen werden uebernommen,
  oder gar keine).
- **Programmlogik bleibt erhalten (der eigentliche Kern der Anfrage)**:
  ein Label ist keine Dekoration - `identOf()`/`rawIdent()` in
  `muicodegen.cpp` verwenden es direkt als generierten C-Bezeichner, und
  sowohl `NotifyEvent::targetLabel` (auf JEDEM Objekt im ganzen Projekt)
  als auch `MuibProject::aboutBox`'s eigenes `aboutLinkedMenuItem`
  referenzieren andere Objekte exakt ueber diesen String, projektweit -
  nicht nur innerhalb des gerade bearbeiteten Fensters. Da der Dialog
  selbst nur eine tiefkopierte Kopie EINES Fensters kennt, kann er
  projektweite Referenzen nicht selbst umschreiben. Deshalb neu:
  `MenuEditorDialog::labelRenames()` liefert nach Annahme des Dialogs
  jede tatsaechlich geaenderte alt->neu-Zuordnung (identitaetsbasiert
  erfasst, sodass auch mehrfaches Umbenennen desselben Eintrags in einer
  Sitzung korrekt nur die Netto-Aenderung liefert), und die neue,
  wiederverwendbare Utility-Funktion `objecttreeutil.h`s
  `applyLabelRenames(MuibProject*, renames)` schreibt danach projektweit
  jedes passende `NotifyEvent::targetLabel` (Fenster-Root-Gruppen UND
  Menue-Baeume UND `proj->appMenu` UND die projektweite
  `MuibProject::notify`-Liste) sowie `aboutBox->aboutLinkedMenuItem` auf
  den neuen Namen um. `MainWindow::onMenuEditor()` ruft das nach jedem
  akzeptierten Dialog automatisch auf.

Verifikation: beide Build-Targets sauber ohne Warnungen neu gebaut; die
bestehende Regressionssuite (15 echte Beispiele + alle synthetischen
Tests) laeuft weiterhin fehlerfrei durch (dieses Feature ist GUI-seitig
und daher nicht Teil der headless `test_codegen`-Suite, siehe unten).
Zusaetzlich ein eigenstaendiges Verifikationsprogramm gegen ein
synthetisches Projekt geschrieben, das `applyLabelRenames()` direkt und
isoliert prueft: eine Umbenennung wird sowohl in einem Gadget im
Fenster-Root-Baum ALS AUCH in einem Geschwister-Menuepunkt im
Menue-Baum ALS AUCH in `proj->appMenu` ALS AUCH in der projektweiten
`proj->notify`-Liste korrekt nachgezogen; ein `aboutLinkedMenuItem`, das
auf ein ANDERES, nicht umbenanntes Label zeigt, bleibt unveraendert und
folgt erst, wenn genau SEIN referenziertes Label umbenannt wird; ein
leeres Rename-Mapping ist ein sicherer No-Op. **ALLE CHECKS BESTANDEN**.

### Feature: Internationalisierung (Englisch/Deutsch), Theme-System, Kommandozeilenparameter

Nutzer-Anforderung: MuiBuilderQt soll (1) Englisch als Quellsprache haben,
mit zur Laufzeit umschaltbarer, gespeicherter deutscher Uebersetzung ueber
ein neues Menue "View/GUI Language" (Englisch/Deutsch, mutual exclusiv,
aktive Sprache mit Punkt markiert); (2) alle Themes von AmigaED 4.0 auch
hier anbieten, ueber ein neues, zur Laufzeit befuelltes Menue "View/Theme",
ebenfalls gespeichert und beim naechsten Start geladen; (3) weiterhin als
eigenstaendige App startbar bleiben, aber zusaetzlich die
Kommandozeilenparameter `--GUI_Language` und `--GUI_Theme` verarbeiten
(fuer den Aufruf aus AmigaED heraus). Wichtige Praezisierung des
Chefentwicklers dazu: ruft AmigaED MuiBuilderQt mit diesen Parametern auf,
sollen sie AmigaEDs eigene, gerade aktive Sprache/Design widerspiegeln,
ohne den in MuiBuilderQt selbst gespeicherten Standard zu ueberschreiben -
**nur fuer diese eine Sitzung** (explizit vom Chefentwickler so bestaetigt).

Umgesetzt, komplett byte-genau nach AmigaED 4.0s eigener, bereits
produktiver Implementierung portiert (gefunden im selben Arbeitsbereich:
`AmigaED_4.0.157.zip` fuer die grundlegende `.pro`/`.ts`/i18n-Struktur, der
neuere, noch nicht gemergte VSCode-Theme-Patch fuer das aktuellste
Theme-System inkl. des 4. synthetischen Themes "Visual Studio Code Dark"):

- **Oberflaechensprache (View > GUI Language)**: `QTranslator` fuer
  `muibuilderqt_de.qm` (eigene Uebersetzung) plus ein zweiter,
  unabhaengiger `QTranslator` fuer Qts eigenes `qtbase_de.qm` - letzterer
  behebt denselben real gemeldeten AmigaED-Bug praeventiv auch hier:
  QMessageBox-Standardbuttons ("&Yes"/"&No"/"OK"/"Cancel"/...) laufen NIE
  durch eigene `tr()`-Aufrufe, sondern durch Qts interne Strings, und
  blieben ohne `qtbase_de.qm` englisch, selbst bei aktivem Deutsch.
  `MainWindow::applyGuiLanguage(code, persist)` installiert/deinstalliert
  beide Translatoren, ruft bei bereits existierendem Menue
  `retranslateUi()` fuer sofortige Live-Umschaltung auf, und persistiert
  optional via `QSettings` (`MISC/DefaultGUILanguage`).
- **Alle 209 uebersetzbaren Strings** der gesamten GUI (vorher zu 100%
  hartkodiertes Deutsch, ohne jede i18n-Infrastruktur) auf Englisch als
  Quellsprache umgestellt und in `translations/muibuilderqt_de.ts`
  vollstaendig ins Deutsche rueckuebersetzt (0 unfinished) - wo dasselbe
  Konzept bereits in AmigaEDs eigener `amigaed_de.ts` uebersetzt vorliegt
  (z.B. "&File"->"&Datei", "&Save"->"&Speichern", "GUI Language"->
  "Oberflaechensprache"), wortgleich uebernommen, fuer eine konsistente
  Terminologie in der ganzen Programmfamilie. `translations.qrc` bindet
  `muibuilderqt_de.qm` + `qtbase_de.qm` (kopiert aus der lokalen
  Qt-Installation, wie bei AmigaED) unter `:/translations` als
  Qt-Ressource ein - garantiert vorhanden, unabhaengig vom Zielsystem.
  Ein bei dieser Umstellung entdecktes Detail aus `objectfactory.cpp`:
  `PaletteEntry.category` diente gleichzeitig als Anzeigetext UND als
  Gruppierungsschluessel (`!=`-Vergleich in `WidgetPalette`) - dafuer
  wurde der Schluessel auf stabile, unuebersetzte englische Literale
  ("Controls"/"Display") umgestellt und eine separate
  `categoryDisplayName()`-Funktion ergaenzt, die nur zur Anzeigezeit
  uebersetzt, damit die Gruppierung sprachunabhaengig korrekt bleibt.
- **`retranslateUi()`**: aktualisiert nach einem Laufzeit-Sprachwechsel
  jedes langlebige Widget (Menuetitel/-eintraege, Dock-Titel,
  Fenster-Umschalter-Toolbar, Fenstertitel). Drei Widgets, die - anders
  als ObjectTreeView/ObjectBox/Recent-Projects, die bei jedem Rebuild
  ohnehin frisch `tr()`en - nur EINMAL aus einer statischen Liste befuellt
  werden und sonst niemals von selbst aktualisiert wuerden, bekamen dafuer
  eine eigene `retranslate()`-Methode: `WidgetPalette` (Kategorie-Header),
  `PropertyInspector` (baut das Formular fuer das aktuell ausgewaehlte
  Objekt neu auf, inkl. Platzhaltertext) und `CanvasWidget` (der
  "Kein Fenster geoeffnet"-Platzhaltertext - dieser dritte Fall wurde erst
  bei der visuellen Verifikation per Xvfb-Screenshot entdeckt, siehe
  unten, und nachtraeglich ergaenzt). Modale Dialoge (AboutBoxDialog,
  ProjectPropertiesDialog, MenuEditorDialog) brauchen keine solche
  Behandlung, da sie bei jedem Oeffnen ohnehin neu konstruiert werden.
- **Theme-System (View > Theme)**, byte-genau aus AmigaED 4.0 portiert:
  `buildThemeMenu()` befuellt das Menue zur Laufzeit mit jedem nativen
  `QStyleFactory::keys()`-Stil, gefolgt von den 4 synthetischen Themes
  "Dark"/"Workbench 1.3"/"Workbench 3.1"/"Visual Studio Code Dark" (exakt
  dieselben QPalette-Farbwerte wie in AmigaED - z.B. Workbench 1.3s Blau
  #0055AA + Orange #FF8800, VSCode Darks #1E1E1E/#252526/#094771), alle in
  einer gemeinsamen `QActionGroup` fuer mutual-exclusive Checkbox-Punkte.
  `applyApplicationStyle()` erzwingt fuer jedes der 4 synthetischen Themes
  den Fusion-Stil + eigene QPalette, plus punktuelle QSS-Stylesheets fuer
  Workbench 1.3s Menueleiste bzw. VSCode Darks Status-/Menue-/Toolbar-
  Chrome (Fusions eigene Farbverlaufs-Synthese aus einer einzelnen
  QPalette-Farbe macht sonst z.B. bei Workbench 1.3s gesaettigtem Blau den
  Menuetext unlesbar). `applyTheme(name, persist)` persistiert optional
  (`MISC/DefaultStyle`) und wendet sofort an.
- **Kommandozeilenparameter** (`main.cpp`, neu mit `QCommandLineParser` -
  AmigaED selbst hat dafuer keine Vorlage, nur eine naive
  `argv[1]`-oeffnet-eine-Datei-Konvention, die als MuiBuilderQts eigenes
  positionelles Argument erhalten blieb): `--GUI_Language <en|de>` und
  `--GUI_Theme <name>`, ausgewertet NACH der Konstruktion von
  `MainWindow` (die bereits MuiBuilderQts eigenen gespeicherten Standard
  laedt) und nur falls tatsaechlich angegeben, jeweils mit
  `persist=false` - reflektiert also AmigaEDs aktuelle Wahl sofort, ohne
  den in MuiBuilderQt selbst gespeicherten Standard fuer den naechsten
  eigenstaendigen Start zu veraendern, exakt wie vom Chefentwickler
  bestaetigt. `MuiBuilderQt --GUI_Language=de --GUI_Theme="Workbench 1.3"
  projekt.MUIB` funktioniert in beliebiger Parameter-Reihenfolge.
  `applyGuiLanguage()`/`applyTheme()` sind dafuer (anders als bei AmigaED,
  das eine separate Prefs-Dialog-Instanz fuer den "echten" Standard hat)
  bewusst public auf `MainWindow`, UND View-Menue-Klicks persistieren hier
  bewusst sofort (`persist=true`) - MuiBuilderQt hat keinen eigenen
  Prefs-Dialog, also ist die View-Menue-Wahl hier selbst schon der
  Standard, anders als bei AmigaEDs eigenem, dort nur sitzungsweitem
  View-Menue-Quick-Switch.

Verifikation: beide Build-Targets von Grund auf neu gebaut (`rm -rf` +
`qmake6` + `make`), 0 Warnungen/Fehler; komplette Regressionssuite (15
echte Beispiele + alle synthetischen Tests inkl. des eigenstaendigen
`applyLabelRenames()`-Verifikationsprogramms) laeuft weiterhin
fehlerfrei durch. `lupdate` fand alle 209 Quellstrings, `lrelease`
kompilierte alle 209 fehlerfrei zu `muibuilderqt_de.qm` (0 unfinished).
Visuelle Verifikation per Xvfb + Screenshots: `--GUI_Language=de
--GUI_Theme=Dark` sowie `--GUI_Theme="Workbench 1.3"` zeigen korrekt
uebersetzte Menues/Docks/Platzhaltertexte und das jeweils korrekt
angewendete Farbschema; View > Oberflaechensprache und View > Design
zeigen die erwarteten Eintraege mit korrekt gesetztem Haekchen/Punkt
(Deutsch bzw. Workbench 1.3 markiert); ein Start ganz ohne
Kommandozeilenparameter (frische, leere QSettings) startet weiterhin wie
zuvor auf Englisch mit Plattform-Standarddesign. Bei dieser visuellen
Pruefung wurde die oben erwaehnte fehlende `CanvasWidget::retranslate()`
entdeckt und sofort nachgezogen (verifiziert per erneutem Screenshot).

### Bugfix: Canvas bei dunklen Themes schwer lesbar + Splitter kaum erkennbar

Nutzer-Meldung (mit zwei Screenshots von `menuTest.MUIB` unter einem
dunklen Design): der Canvas-Bereich zur Fenster-/Gadget-Erstellung ist
bei Verwendung der dunklen Themes nur schwer lesbar; zusaetzlich sind die
Splitter zwischen Projektbaum, Canvas-Bereich und Eigenschaften nur an
den gepunkteten Greifern erkennbar.

Zwei getrennte Root Causes gefunden und behoben:

- **Canvas-Boxen (ObjectBox)**: `colorForType()` liefert bewusst immer
  eine helle Pastellfarbe pro Objekt-Kategorie (soll wie eine kleine,
  theme-unabhaengige Amiga-Mockup-Vorschau wirken), aber nur
  `QPalette::Window` wurde je gesetzt - die Kopfzeilen-Beschriftung
  (`m_headerLabel`) uebernahm ihre Textfarbe dagegen ueber die normale
  QSS-Kaskade, die - sobald IRGENDEIN Vorfahre (hier: `MainWindow` bei 3
  der 4 synthetischen Themes) ein eigenes Stylesheet gesetzt hat - fuer
  ein unstyled Label die Farbe aus der anwendungsweiten Palette zieht statt
  aus der lokal ueberschriebenen des Eltern-Widgets - z.B. Darks helles
  `#d4d4d4`. Helle Schrift auf hellem Pastell-Hintergrund war exakt das
  gemeldete Lesbarkeitsproblem. Behoben, indem die Textfarbe direkt ins
  eigene Stylesheet von `m_headerLabel` einkompiliert wird (`color:
  #202020;`), unabhaengig vom Stylesheet-Zustand jedes Vorfahren.
- **Splitter/Separator**: keines der 4 synthetischen QPalettes setzt die
  Rollen `Light`/`Midlight`/`Dark`/`Mid`/`Shadow` (nur die von AmigaEDs
  eigenen Palette-Funktionen uebernommenen Rollen - byte-genauer Port).
  Qt leitet diese fehlenden Rollen automatisch von `Window`/`Button` ab -
  für eine helle Basisfarbe unproblematisch, aber für eine na­hezu
  schwarze Basis (Dark: `#353535`, Visual Studio Code Dark: `#252526`)
  komprimieren die abgeleiteten Schattierungen ebenfalls Richtung Schwarz,
  wodurch Fusions Separator-Rendering (das genau auf diesen Rollen
  basiert) praktisch keinen Kontrast mehr zur Umgebung hat. Da dies keine
  AmigaED-Entsprechung hat (reines MuiBuilderQt-Problem, kein Portierungs-
  Gegenstueck), wurde stattdessen jedem der 4 Themes eine eigene, klar
  sichtbare `QMainWindow::separator`-Stylesheet-Regel spendiert (Dark:
  `#5a5a5a`, Visual Studio Code Dark: `#6e6e6e`, Workbench 1.3: `#CCCCCC`,
  Workbench 3.1: `#666666` - jeweils mit ausreichendem Kontrast zur
  eigenen Hintergrundfarbe), statt zu raten, welche der 4 Themes im
  Einzelnen betroffen sind.

Verifikation: exakt das vom Nutzer gemeldete Projekt (`menuTest.MUIB`)
unter `--GUI_Theme=Dark` und `--GUI_Theme="Visual Studio Code Dark"` per
Xvfb + Screenshot (inkl. gezoomtem Ausschnitt) geladen - Canvas-Text jetzt
in beiden Themes klar lesbar (identisch zur nativen/hellen Standard-
Darstellung), Splitter als klar sichtbarer heller Balken statt nur an den
Greifpunkten erkennbar. Beide Build-Targets von Grund auf neu gebaut, 0
Warnungen/Fehler; komplette Regressionssuite weiterhin ALL CHECKS PASSED
(reine GUI-Aenderung, core/ unberuehrt).

### Manuell getestet (Xvfb, headless)

Ein reales Beispielprojekt (`BuilderSave/Small.MUIB`) geladen, per
simuliertem Drag&Drop einen dritten Button in eine bestehende Gruppe
gezogen (korrekt dort eingefuegt, automatisch eindeutig benannt,
sofort ausgewaehlt), dessen Beschriftung im Property-Inspector
bearbeitet (Aenderung erschien sofort im Canvas-Header, ohne
Neuaufbau), und die Synchronisation Projektbaum <-> Canvas <->
Inspector in beide Richtungen bestaetigt. Sauberer qmake6+make-Build,
keine Fehler, keine Compiler-Warnungen.

Zusaetzlich fuer das Drag-Umsortieren verifiziert: `BT_cancel` per Drag
vor `BT_ok` innerhalb von `GR_grp_1` gezogen - Canvas UND Projektbaum
zeigen danach sofort die neue Reihenfolge (`BT_cancel`, `BT_ok`),
Auswahl bleibt auf dem verschobenen Objekt. Anschliessend versucht,
`BT_ok` in die andere Group (`GR_lists`) zu ziehen - Drop wird
korrekt abgelehnt, Baum bleibt unveraendert (nur die Selektion
wechselt durch den Mausklick selbst). Sauberer qmake6+make-Build,
keine Fehler, keine Compiler-Warnungen; keine Auffaelligkeiten im
Log waehrend der gesamten Session.

Zusaetzlich fuer "Letzte Projekte" verifiziert (frisches `HOME`, damit
der leere Zustand garantiert ohne Vorbelegung startet): Datei-Menue
zeigt zunaechst den deaktivierten Platzhalter "(keine letzten
Projekte)"; `Small.MUIB` per Kommandozeilen-Pfad geladen (uebt denselben
`loadProjectFile()`-Pfad wie Datei/Oeffnen... aus) - Eintrag erscheint
sofort im Untermenue; App beendet und mit demselben `HOME` neu
gestartet (ohne Kommandozeilen-Pfad) - Eintrag ist weiterhin vorhanden
(QSettings-Persistenz ueber `~/.config/MB-SoftWorX/MuiBuilderQt.conf`
direkt bestaetigt); Klick auf den Eintrag laedt das Projekt korrekt neu
(Canvas, Fenster-Umschalter und Property-Inspector zeigen wieder den
vollstaendigen Projektinhalt); ein zweiter, kuenstlich auf einen nicht
mehr existierenden Pfad gesetzter Eintrag erscheint erwartungsgemaess
zuerst (neuestes-zuerst-Sortierung) und wird beim Anklicken mit der
korrekten Warnmeldung entfernt (Liste UND QSettings-Datei bestaetigt
aktualisiert); "Liste leeren" setzt das Untermenue zuverlaessig auf den
Platzhalter zurueck. Sauberer qmake6+make-Build, keine Fehler, keine
Compiler-Warnungen.

### Build (GUI)

```
cd gui
qmake6 MuiBuilderQt.pro && make
./MuiBuilderQt [optional: Pfad zu einer .MUIB-Datei]
```
