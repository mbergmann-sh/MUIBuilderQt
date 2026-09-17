; MuiBuilderQt Setup script (Inno Setup)
;
; Expected layout when this file is compiled:
;   MuiBuilderQt_install\MuiBuilderQt.iss   <- this file
;   MuiBuilderQt_install\install_src\       <- MuiBuilderQt.exe, all Qt DLLs
;                                               and plugin subfolders, staged
;                                               automatically by
;                                               gui\MuiBuilderQt.pro's win32
;                                               build step after every
;                                               successful build
;
; Compile with the Inno Setup Compiler (ISCC.exe) or by opening this file in
; the Inno Setup IDE and choosing Build > Compile. The result is a single
; MuiBuilderQt_Setup.exe in MuiBuilderQt_install\Output\.
;
; Structurally modeled on AmigaED's own AmigaED_install\AmigaED.iss (same
; author/publisher, same installer mechanisms) - see that file for the more
; extensively commented original. This version drops everything specific to
; AmigaED that doesn't apply here (documentation/examples install prompts,
; the AmigaED-specific disclaimer wording - see the [CustomMessages] section
; below) and keeps the reusable parts: the license-disclaimer gate, the
; "already installed?" reinstall/update/uninstall choice, and the Welcome
; page/wizard-image handling.
;
; IMPORTANT: AppId below is a fixed GUID, freshly generated for THIS program
; (NOT the same GUID as AmigaED.iss's own AppId - a different application
; must never share one) - it identifies "MuiBuilderQt" to Windows' installed
; -programs registry across versions. Do NOT regenerate it for future
; releases - a changed AppId would make Windows (and this script's own
; "already installed?" check) treat a new build as a completely different,
; unrelated program, breaking upgrades/uninstalls of anything installed
; with an earlier AppId.

#define MyAppName "MuiBuilderQt"
; Placeholder - MuiBuilderQt has no established version-number scheme yet
; (no version.h/equivalent anywhere in the codebase, unlike AmigaED's own
; Revisions.md + version.h). Adjust freely; just keep it in sync with
; OutputBaseFilename below if you start stamping real release numbers.
#define MyAppVersion "1.0"
#define MyAppPublisher "MB-SoftWorX"
#define MyAppExeName "MuiBuilderQt.exe"
#define MyAppId "3C3B3F82-576C-4F5B-9CA7-6076C2EAEC67"

; Whether wizard_image.png (see the WizardImageFile comment further below)
; is available to also show inside the license disclaimer dialog in [Code].
; Defined at compile time so both the [Files] entry that stages it into
; {tmp} and the Pascal code that loads it can be left out together,
; cleanly, if the file isn't there.
#if FileExists(SourcePath + "wizard_image.png")
  #define HasDisclaimerImage
#endif

[Setup]
AppId={{{#MyAppId}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
; C:\MuiBuilderQt - matches the default Tool-1 path AmigaED's own Prefs >
; Tools dialog already ships with, so a fresh MuiBuilderQt install via this
; installer works out of the box with AmigaED's default GUI-builder tool
; configuration, with no manual path adjustment needed on either side.
; Deliberately NOT the standard "{autopf}\..." Program Files suggestion,
; matching AmigaED.iss's own approach.
DefaultDirName=C:\MuiBuilderQt
; Let the user change the path on the wizard's directory-selection page
; instead of just accepting the default silently.
DisableDirPage=no
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
; The staged output is the sole source tree for [Files] below - every
; relative Source: path is resolved against this folder.
SourceDir=install_src
; Relative directives (this one included) resolve against SourceDir, not
; against this .iss file's own folder - so "..\Output" lands at
; MuiBuilderQt_install\Output, a sibling of install_src. Deliberately NOT
; "Output" (i.e. install_src\Output): install_stage.bat wipes install_src
; completely on every build (see gui\MuiBuilderQt.pro), which would delete
; any previously compiled Setup.exe sitting inside it right along with the
; rest.
OutputDir=..\Output
OutputBaseFilename=MuiBuilderQt_Setup
Compression=lzma
SolidCompression=yes
WizardStyle=modern
; gui\MuiBuilderQt.pro's win32 build targets 64-bit MinGW; matches
; windeployqt's own output, which is 64-bit-only (no 32-bit DLLs staged).
ArchitecturesInstallIn64BitMode=x64compatible
; Writing to C:\MuiBuilderQt (outside the user's own profile) needs
; elevation on a standard Windows setup, same as most installers that
; don't target {localappdata}/{userappdata}.
PrivilegesRequired=admin
UninstallDisplayIcon={app}\{#MyAppExeName}
; Shown on the left of the Welcome and Finished pages (164x314 px) and,
; scaled down, top-right on every other page (55x58 px). Place your own
; wizard_image.png / wizard_image_small.png directly in this folder
; (MuiBuilderQt_install, next to this .iss file) - install_src itself gets
; wiped and rebuilt on every compile, so an image dropped in there wouldn't
; survive the next build. Each directive only takes effect if its file
; actually exists, so compiling still works even if you've only supplied
; one of the two (or neither yet).
#if FileExists(SourcePath + "wizard_image.png")
WizardImageFile={#SourcePath}wizard_image.png
#endif
#if FileExists(SourcePath + "wizard_image_small.png")
WizardSmallImageFile={#SourcePath}wizard_image_small.png
#endif
; The Welcome page (shown first, before any file copying) already shows the
; large WizardImageFile above and offers exactly "Next" (proceed with the
; install) or "Cancel" (abort, nothing changed) - Inno Setup's built-in
; behaviour, not something that needs building by hand.
DisableWelcomePage=no

[Languages]
Name: "german"; MessagesFile: "compiler:Languages\German.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
; Used by the custom "already installed" dialog (AskInstallChoice in [Code]
; below) instead of a plain Yes/No/Cancel MsgBox - a stock MsgBox can't
; carry custom button captions, and a real 3-way choice (Reinstall / Update
; / Uninstall) needs more than Yes/No/Cancel provides anyway. Same reusable
; mechanism/wording as AmigaED.iss.
english.AlreadyInstalledCaption=%1 is already installed
english.AlreadyInstalledText=What would you like to do?
english.BtnReinstall=&Reinstall (keep old files, install over them)
english.BtnUpdate=&Update (remove the old version first, then install fresh)
english.BtnUninstall=&Uninstall %1
english.BtnCancel=Cancel
english.UninstallFailed=The uninstallation could not be started.
german.AlreadyInstalledCaption=%1 ist bereits installiert
german.AlreadyInstalledText=Was möchten Sie tun?
german.BtnReinstall=&Neu installieren (alte Dateien behalten, drüberinstallieren)
german.BtnUpdate=&Update (alte Version zuerst entfernen, dann neu installieren)
german.BtnUninstall=%1 &deinstallieren
german.BtnCancel=Abbrechen
german.UninstallFailed=Die Deinstallation konnte nicht gestartet werden.

; Used by the license disclaimer dialog (ShowDisclaimer in [Code] below),
; shown right after Setup's language-selection page and before anything
; else - including the "already installed?" check further down.
;
; NOTE: unlike AmigaED.iss's own DisclaimerText (which carries the AmigaED
; author's personal, project-specific political statement), this text was
; deliberately written neutral and factual for MuiBuilderQt: it states the
; real, known facts (Qt6/C++ port of Darren Coles' MUI-Builder v2.3, made
; with his permission, provided as-is) and invents no formal license claim,
; since MuiBuilderQt's own repository doesn't state one. Adjust freely if
; you want different wording or an actual license statement once one exists.
english.DisclaimerCaption=Before you continue
english.DisclaimerText=MuiBuilderQt is a Qt6/C++ port of Darren Coles' MUI-Builder (v2.3), created with his permission as a sub-project of AmigaED.%n%nIt is provided as-is, without warranty of any kind.%n%nMichael Bergmann%n%nAuthor of this port
english.BtnAccept=&Continue
english.BtnDecline=&Cancel
german.DisclaimerCaption=Bevor Sie fortfahren
german.DisclaimerText=MuiBuilderQt ist ein Qt6/C++-Port von Darren Coles' MUI-Builder (v2.3), mit seiner Erlaubnis entstanden als Teilprojekt von AmigaED.%n%nEs wird ohne jede Gewährleistung bereitgestellt.%n%nMichael Bergmann%n%nAutor dieses Ports
german.BtnAccept=&Fortfahren
german.BtnDecline=&Abbrechen

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; Everything staged by the build (the .exe, every Qt DLL, and every plugin
; subfolder windeployqt created) - copied as-is, subfolder structure
; preserved. Unlike AmigaED.iss, there is no DOC/Examples carve-out - the
; whole install_src tree is copied unconditionally.
Source: "*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion
; Staged into {tmp} (not {app} - never installed alongside the program)
; purely so the license disclaimer dialog in [Code] can load it at
; runtime. Only present if HasDisclaimerImage was defined above.
#ifdef HasDisclaimerImage
Source: "{#SourcePath}wizard_image.png"; DestDir: "{tmp}"; Flags: dontcopy noencryption
#endif

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; Belt-and-braces cleanup: removes the install folder even if it still
; contains files [Files] above didn't track, so a reinstall always starts
; from a clean folder.
Type: filesandordirs; Name: "{app}"

; --- Install-or-uninstall-or-update prompt -----------------------------
; Runs before the wizard shows any page. If MuiBuilderQt is already
; installed (its uninstall registry key exists), asks the user what to do
; instead of just barrelling into a reinstall: reinstall over the top, do a
; clean update (uninstall first, then install fresh), or uninstall only.
; A plain uninstall or a cancelled dialog exits without ever showing the
; install wizard. Same mechanism as AmigaED.iss's own [Code] section, minus
; the DOC/Examples install prompts at the end (no equivalent assets exist
; for MuiBuilderQt).
[Code]
var
  // Set by the button click handlers below; AskInstallChoice() reads it
  // back once the dialog closes. 1 = reinstall over the top, 2 = clean
  // update (uninstall then reinstall), 3 = uninstall only, 0 = cancel
  // (including the dialog being closed via its title bar/Esc).
  InstallChoiceResult: Integer;
  InstallChoiceForm: TSetupForm;
  // Running Y position while AddInstallChoiceButton stacks buttons onto
  // InstallChoiceForm - a plain global instead of a local var, since Inno
  // Setup's Pascal Script does not support nested functions.
  InstallChoiceBtnTop: Integer;

  // Set by the button click handlers below; ShowDisclaimer() reads it
  // back once the dialog closes. True = Accept/Continue (installation
  // continues), False = Decline/Cancel or the dialog closed any other way
  // (Esc/title bar) - same "closing it any other way counts as the
  // safe/negative choice" convention as InstallChoiceResult above.
  DisclaimerAccepted: Boolean;
  DisclaimerForm: TSetupForm;

// One shared handler for all four buttons - simpler than four near-
// identical procedures, since every button already carries its own result
// value via its Tag property (set where each button is created in
// AddInstallChoiceButton below).
procedure InstallChoiceButtonClick(Sender: TObject);
begin
  InstallChoiceResult := TNewButton(Sender).Tag;
  InstallChoiceForm.Close;
end;

// Creates one button on InstallChoiceForm, stacked below the previous one
// via InstallChoiceBtnTop, wired to AResultValue through its Tag.
function AddInstallChoiceButton(const ACaption: String; AResultValue: Integer): TNewButton;
begin
  Result := TNewButton.Create(InstallChoiceForm);
  Result.Parent := InstallChoiceForm;
  Result.Left := ScaleX(16);
  Result.Top := InstallChoiceBtnTop;
  Result.Width := InstallChoiceForm.ClientWidth - ScaleX(32);
  Result.Height := ScaleY(23);
  Result.Caption := ACaption;
  Result.Tag := AResultValue;
  Result.OnClick := @InstallChoiceButtonClick;
  InstallChoiceBtnTop := InstallChoiceBtnTop + ScaleY(30);
end;

// Small custom dialog replacing the old three-way Yes/No/Cancel MsgBox: a
// stock MsgBox can't carry custom button captions, and a real 3-way choice
// (Reinstall / Update / Uninstall) plus Cancel needs more than Yes/No/
// Cancel provides. All captions come from [CustomMessages] above, so this
// always shows in whichever language the user picked on Setup's
// language-selection page.
function AskInstallChoice(): Integer;
var
  Btn: TNewButton;
  InfoLabel: TNewStaticText;
begin
  InstallChoiceResult := 0; // closing the form any other way still counts as Cancel

  // CreateCustomForm's signature since Inno Setup 6.6.0: ClientWidth/
  // ClientHeight are constructor parameters (read-only afterwards), plus
  // KeepSizeX/KeepSizeY (whether the form may grow with
  // WizardSizePercent) - False/False keeps this dialog a fixed size.
  InstallChoiceForm := CreateCustomForm(ScaleX(420), ScaleY(210), False, False);
  try
    InstallChoiceForm.Caption := ExpandConstant('{cm:AlreadyInstalledCaption,{#MyAppName}}');
    InstallChoiceForm.Position := poScreenCenter;
    InstallChoiceForm.BorderStyle := bsDialog;

    InfoLabel := TNewStaticText.Create(InstallChoiceForm);
    InfoLabel.Parent := InstallChoiceForm;
    InfoLabel.Left := ScaleX(16);
    InfoLabel.Top := ScaleY(16);
    InfoLabel.Width := InstallChoiceForm.ClientWidth - ScaleX(32);
    InfoLabel.AutoSize := False;
    InfoLabel.WordWrap := True;
    InfoLabel.Caption := ExpandConstant('{cm:AlreadyInstalledText}');

    InstallChoiceBtnTop := ScaleY(60);
    AddInstallChoiceButton(ExpandConstant('{cm:BtnReinstall}'), 1);
    AddInstallChoiceButton(ExpandConstant('{cm:BtnUpdate}'), 2);
    Btn := AddInstallChoiceButton(ExpandConstant('{cm:BtnUninstall,{#MyAppName}}'), 3);
    Btn := AddInstallChoiceButton(ExpandConstant('{cm:BtnCancel}'), 0);
    Btn.Cancel := True; // Esc / closing the dialog acts as Cancel

    InstallChoiceForm.ActiveControl := Btn;
    InstallChoiceForm.ShowModal;
  finally
    InstallChoiceForm.Free;
  end;

  Result := InstallChoiceResult;
end;

// --- License disclaimer dialog -----------------------------------------
// Shown right after the language-selection page, before Setup does
// anything else. Two buttons, Continue/Cancel (captions from
// [CustomMessages] above, so they follow the chosen Setup language) -
// deliberately a custom form rather than a stock MsgBox, since a plain
// MsgBox's OK/Cancel captions can't be renamed.
procedure DisclaimerButtonClick(Sender: TObject);
begin
  DisclaimerAccepted := (TNewButton(Sender).Tag = 1);
  DisclaimerForm.Close;
end;

function ShowDisclaimer(): Boolean;
var
  InfoLabel: TNewStaticText;
  AcceptBtn, DeclineBtn: TNewButton;
  BtnWidth: Integer;
  TextLeft: Integer;
  ContentBottom: Integer;
#ifdef HasDisclaimerImage
  DisclaimerImage: TBitmapImage;
#endif
begin
  DisclaimerAccepted := False; // closing the form any other way still counts as Decline

  // Width is the one fixed number here; height is never hand-guessed:
  // InfoLabel below computes its own exact wrapped height for whatever
  // text/font/DPI this particular system renders, and the form is resized
  // to fit that plus the button row right after. Placeholder height here
  // only - overwritten via ClientHeight below.
  DisclaimerForm := CreateCustomForm(ScaleX(460), ScaleY(300), False, False);
  try
    DisclaimerForm.Caption := ExpandConstant('{cm:DisclaimerCaption}');
    DisclaimerForm.Position := poScreenCenter;
    DisclaimerForm.BorderStyle := bsDialog;

#ifdef HasDisclaimerImage
    DisclaimerImage := TBitmapImage.Create(DisclaimerForm);
    DisclaimerImage.Parent := DisclaimerForm;
    DisclaimerImage.Left := ScaleX(16);
    DisclaimerImage.Top := ScaleY(16);
    DisclaimerImage.Width := ScaleX(110);
    DisclaimerImage.Stretch := True;
    DisclaimerImage.Center := True;
    ExtractTemporaryFile('wizard_image.png');
    DisclaimerImage.PngImage.LoadFromFile(ExpandConstant('{tmp}\wizard_image.png'));
    TextLeft := DisclaimerImage.Left + DisclaimerImage.Width + ScaleX(16);
#else
    TextLeft := ScaleX(16);
#endif

    InfoLabel := TNewStaticText.Create(DisclaimerForm);
    InfoLabel.Parent := DisclaimerForm;
    InfoLabel.Left := TextLeft;
    InfoLabel.Top := ScaleY(16);
    InfoLabel.AutoSize := False;
    InfoLabel.WordWrap := True;
    // Width fixed first, Caption set next, AutoSize turned on last: with
    // WordWrap already True and a Width already in place, AutoSize now
    // only grows/shrinks Height to exactly fit the wrapped text.
    InfoLabel.Width := DisclaimerForm.ClientWidth - TextLeft - ScaleX(16);
    InfoLabel.Caption := ExpandConstant('{cm:DisclaimerText}');
    InfoLabel.AutoSize := True;

    ContentBottom := InfoLabel.Top + InfoLabel.Height;

#ifdef HasDisclaimerImage
    DisclaimerImage.Height := InfoLabel.Height;
    if DisclaimerImage.Top + DisclaimerImage.Height > ContentBottom then
      ContentBottom := DisclaimerImage.Top + DisclaimerImage.Height;
#endif

    // Resize the form to exactly fit what's above plus one button row.
    DisclaimerForm.ClientHeight := ContentBottom + ScaleY(16) + ScaleY(23) + ScaleY(20);

    // Fixed, compact width (not "half the dialog each"), right-aligned as
    // a pair, Windows-dialog-style.
    BtnWidth := ScaleX(130);

    DeclineBtn := TNewButton.Create(DisclaimerForm);
    DeclineBtn.Parent := DisclaimerForm;
    DeclineBtn.Left := DisclaimerForm.ClientWidth - ScaleX(16) - BtnWidth;
    DeclineBtn.Top := ContentBottom + ScaleY(16);
    DeclineBtn.Width := BtnWidth;
    DeclineBtn.Height := ScaleY(23);
    DeclineBtn.Caption := ExpandConstant('{cm:BtnDecline}');
    DeclineBtn.Tag := 0;
    DeclineBtn.OnClick := @DisclaimerButtonClick;
    DeclineBtn.Cancel := True; // Esc / closing the dialog acts as Decline

    AcceptBtn := TNewButton.Create(DisclaimerForm);
    AcceptBtn.Parent := DisclaimerForm;
    AcceptBtn.Left := DeclineBtn.Left - ScaleX(10) - BtnWidth;
    AcceptBtn.Top := DeclineBtn.Top;
    AcceptBtn.Width := BtnWidth;
    AcceptBtn.Height := ScaleY(23);
    AcceptBtn.Caption := ExpandConstant('{cm:BtnAccept}');
    AcceptBtn.Tag := 1;
    AcceptBtn.OnClick := @DisclaimerButtonClick;

    // Decline is the active/default control - closing the dialog any way
    // other than an explicit Accept/Continue click is the safe outcome.
    DisclaimerForm.ActiveControl := DeclineBtn;
    DisclaimerForm.ShowModal;
  finally
    DisclaimerForm.Free;
  end;

  Result := DisclaimerAccepted;
end;

function InitializeSetup(): Boolean;
var
  UninstallString: String;
  UninstallRegKey: String;
  ResultCode: Integer;
  Choice: Integer;
begin
  Result := True;

  // License disclaimer - shown first, right after the language-selection
  // page. Declining exits Setup immediately, before the "already
  // installed?" check or anything else gets a chance to run.
  if not ShowDisclaimer() then
  begin
    Result := False;
    Exit;
  end;

  UninstallRegKey := 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{{#MyAppId}}_is1';

  if RegQueryStringValue(HKLM, UninstallRegKey, 'UninstallString', UninstallString) or
     RegQueryStringValue(HKCU, UninstallRegKey, 'UninstallString', UninstallString) then
  begin
    Choice := AskInstallChoice();
    UninstallString := RemoveQuotes(UninstallString);

    case Choice of
      1:
        begin
          // Reinstall over the top: Result stays True, falls through into
          // the normal install wizard, same as before.
        end;

      2:
        begin
          // Clean update: run the existing uninstaller silently first
          // (/VERYSILENT hides its UI so this reads as one continuous
          // operation, not two separate installer windows), then fall
          // through into the normal install wizard exactly like case 1.
          if not Exec(UninstallString, '/VERYSILENT', '', SW_HIDE, ewWaitUntilTerminated, ResultCode) then
          begin
            MsgBox(ExpandConstant('{cm:UninstallFailed}'), mbError, MB_OK);
            Result := False;
          end;
        end;

      3:
        begin
          // Uninstall only - hand off to the existing (visible) uninstaller
          // and exit without ever showing the install wizard.
          if not Exec(UninstallString, '', '', SW_SHOW, ewWaitUntilTerminated, ResultCode) then
          begin
            MsgBox(ExpandConstant('{cm:UninstallFailed}'), mbError, MB_OK);
          end;
          Result := False;
        end;

    else // 0 = cancel
      Result := False;
    end;
  end;
end;
