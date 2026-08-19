# StellarSystem v6.7 Makefile
# Usage: make <target>

UE_ROOT ?= /home/ue4/UnrealEngine
PROJECT = StellarSystem
PROJECT_DIR = $(shell pwd)

.PHONY: help editor client server clean zip verify count

help:
	@echo "StellarSystem v6.7 - Available targets:"
	@echo "  make editor    - Generate project files + build editor"
	@echo "  make client    - Build client (Win64 Shipping)"
	@echo "  make server    - Build server (Win64 Shipping, headless)"
	@echo "  make zip       - Package everything into a zip"
	@echo "  make verify    - Verify file integrity"
	@echo "  make count     - Count lines of code"
	@echo "  make clean     - Clean build artifacts"

editor:
	$(UE_ROOT)/Engine/Build/BatchFiles/Linux/GenerateProjectFiles.sh \
		-project="$(PROJECT_DIR)/$(PROJECT).uproject" -game
	make -C "$(UE_ROOT)/Engine/Source" $(PROJECT)Editor \
		UE4_TARGET=Editor UE4_CONFIG=Development

client:
	$(UE_ROOT)/Engine/Build/BatchFiles/Linux/Build.sh \
		$(PROJECT)Client Win64 Shipping \
		-project="$(PROJECT_DIR)/$(PROJECT).uproject" \
		-progress -threads=$(shell nproc)

server:
	$(UE_ROOT)/Engine/Build/BatchFiles/Linux/Build.sh \
		$(PROJECT)Server Win64 Shipping \
		-project="$(PROJECT_DIR)/$(PROJECT).uproject" \
		-progress -threads=$(shell nproc) -nullrhi

zip: clean
	@echo "Creating StellarSystem_v6.7.zip..."
	cd .. && zip -r StellarSystem_v6.7.zip StellarSystem_v6.7/ \
		-x "*.DS_Store" -x "*.git*" -x "*/Binaries/*" \
		-x "*/Intermediate/*" -x "*/Saved/*"
	@echo "Done: ../StellarSystem_v6.7.zip"

verify:
	@echo "=== Verifying project structure ==="
	@test -f StellarSystem.uproject && echo "  ✅ .uproject exists" || echo "  ❌ .uproject MISSING"
	@test -f Source/StellarSystem/StellarSystem.Build.cs && echo "  ✅ Build.cs exists" || echo "  ❌ Build.cs MISSING"
	@test -f Source/StellarSystemClient.Target.cs && echo "  ✅ Client.Target.cs exists" || echo "  ❌ Client.Target.cs MISSING"
	@test -f Source/StellarSystemServer.Target.cs && echo "  ✅ Server.Target.cs exists" || echo "  ❌ Server.Target.cs MISSING"
	@test -f Source/StellarSystemEditor.Target.cs && echo "  ✅ Editor.Target.cs exists" || echo "  ❌ Editor.Target.cs MISSING"
	@test -f Source/StellarSystem/Public/Client/StellarClientGameMode.h && echo "  ✅ Client GM exists" || echo "  ❌ Client GM MISSING"
	@test -f Source/StellarSystem/Public/Server/StellarDedicatedServer.h && echo "  ✅ Server GM exists" || echo "  ❌ Server GM MISSING"
	@test -f Config/Server.ini && echo "  ✅ Server.ini exists" || echo "  ❌ Server.ini MISSING"
	@test -f Docs/CLIENT_SERVER_SPLIT.md && echo "  ✅ Split doc exists" || echo "  ❌ Split doc MISSING"
	@test -f Docs/SERVER_DEPLOYMENT.md && echo "  ✅ Deploy doc exists" || echo "  ❌ Deploy doc MISSING"
	@test -f Docs/SERVER_ADMIN_GUIDE.md && echo "  ✅ Admin doc exists" || echo "  ❌ Admin doc MISSING"
	@test -f Server/Build/RunServer.sh && echo "  ✅ RunServer.sh exists" || echo "  ❌ RunServer.sh MISSING"
	@test -f Server/Build/BuildServer.sh && echo "  ✅ BuildServer.sh exists" || echo "  ❌ BuildServer.sh MISSING"
	@test -f Client/Build/PackageClient.bat && echo "  ✅ PackageClient.bat exists" || echo "  ❌ PackageClient.bat MISSING"
	@echo "=== Verification complete ==="

count:
	@echo "=== Code statistics ==="
	@echo "  Headers (.h):   $$(find Source -name '*.h' | wc -l) files"
	@echo "  Sources (.cpp): $$(find Source -name '*.cpp' | wc -l) files"
	@echo "  Header lines:   $$(find Source -name '*.h' -exec cat {} + | wc -l)"
	@echo "  Source lines:   $$(find Source -name '*.cpp' -exec cat {} + | wc -l)"
	@echo "  Docs (.md):     $$(find Docs -name '*.md' | wc -l) files"
	@echo "  Doc lines:      $$(find Docs -name '*.md' -exec cat {} + | wc -l)"
	@echo "  Scripts:        $$(find . -name '*.sh' -o -name '*.bat' | wc -l) files"
	@echo "  Total files:    $$(find . -type f | wc -l)"

clean:
	@echo "Cleaning build artifacts..."
	rm -rf Binaries/ Intermediate/ Saved/ .vs/ *.suo
	@echo "Done."
