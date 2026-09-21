polaris troubleshooting
----

fixes polaris loader errors that can be safely automated. you tell it which error code the loader gave you, it runs the fix and reports what it did.

only auto-fixable codes appear in the list. codes that need network, credentials, a driver reinstall, or human judgment are out of scope and belong in support.

## use

download the latest `troubleshooting.exe` from releases and run it from a terminal:

```
troubleshooting.exe
```

pick the code the loader gave you, press auto fix, follow the on-screen result (usually "sign in again" or "retry inject").

## supported codes

- **M014** `m_drv_session_invalid` - injection session not authenticated → delete `ratchet.dat`

more codes are added per release. see closed prs on the repo for what landed when.

## out of scope

anything network-dependent (M008, M015, M019), credential-dependent (A-family), license-state (L-family), gpu / overlay init (O-family), kernel-side (M010, M011, M013). these need attention that a filesystem cleanup cannot give.

## build from source

visual studio 2022, `PlatformToolset=v145`, `stdcpplatest`, vcpkg manifest, triplet `x64-windows-static-md`.

```
git clone https://github.com/polaris-private/troubleshooting.git
cd troubleshooting
msbuild troubleshooting.slnx /p:Configuration=Release /p:Platform=x64
```

vcpkg pulls `ftxui` on first configure.

## layout

```
troubleshooting/
  troubleshooting.slnx           solution
  vcpkg.json                     manifest (ftxui)
  troubleshooting/               msvc project
    main.cpp                     entry, calls ts::ui::run()
    core/                        code enum + registry + fix result / context
    platform/                    filesystem + windows helpers
    fixes/                       one file per auto-fix (function ptr wired in registry)
    ui/                          ftxui shell
```

## report

open an issue on the repo with the code, the loader log around the failure, and what the tool showed you.

## license

mit — see [LICENSE](LICENSE).
