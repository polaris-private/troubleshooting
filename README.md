polaris troubleshooting
----

fixes common polaris loader errors. you tell it which error code you got, it either applies the fix for you or walks you through the manual steps.

## use

download the latest `troubleshooting.exe` from releases. open a terminal in the folder and run it:

```
troubleshooting.exe
```

pick the code the loader gave you (e.g. `M014`), pick **auto fix** or **manual steps**. auto fix runs immediately and reports what it did. manual steps prints the commands / paths you need to touch.

## codes with auto fix

- **T001..T005** — debugger detected → closes known debuggers, then relaunch the loader
- **P004** — tamper mismatch → clears loader cache, forces updater on next launch
- **M003** — config read failed → resets config to defaults
- **M009** — driver binary invalid → clears cached driver blob, next launch re-downloads
- **M014** — session not authenticated → deletes `ratchet.dat`, sign in again
- **M016** — update swap failed → clears the pending update
- **M017** — update signature failed → clears the update files, forces re-download
- **M018** — no product selected → auto-selects if you own only one product
- **M020** — module binary invalid → clears cached module blob, next launch re-downloads

## codes without auto fix (manual only)

everything else in the loader error list. the tool tells you what the code means and what to check (network, license state, hwid, etc.). when a code needs the panel or support, it opens the right page for you.

## build from source

visual studio 2022, `PlatformToolset=v145`, `stdcpplatest`, vcpkg manifest.

```
git clone <repo>
cd troubleshooting
msbuild troubleshooting.slnx /p:Configuration=Release /p:Platform=x64
```

vcpkg pulls `ftxui` on first configure.

## report

open an issue on the repo with the code, the loader log around the failure, and what the tool showed you.

## license

mit — see [LICENSE](LICENSE).
