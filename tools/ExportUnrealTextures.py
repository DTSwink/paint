# Export Unreal Engine Texture2D assets to PNG for Shader Viewer
#
# Usage inside Unreal Editor:
#   1. Open your project (e.g. Berserk)
#   2. Window -> Developer Tools -> Output Log
#   3. Run: py "C:/Users/singerie/Documents/Cursor/temp/ShaderViewer/tools/ExportUnrealTextures.py"
#
# Or from command line (requires editor path):
#   UnrealEditor-Cmd.exe "C:/Users/singerie/Documents/Cursor/Berserk/Berserk.uproject" \
#     -ExecutePythonScript="C:/Users/singerie/Documents/Cursor/temp/ShaderViewer/tools/ExportUnrealTextures.py"
#
# Exports PNG files to:
#   C:/Users/singerie/Documents/Cursor/Berserk/Saved/ShaderViewerExports

import os
import unreal

EXPORT_ROOT = r"C:\Users\singerie\Documents\Cursor\Berserk\Saved\ShaderViewerExports"
CONTENT_ROOT = "/Game"


def ensure_dir(path):
    if not os.path.isdir(path):
        os.makedirs(path, exist_ok=True)


def sanitize_name(name):
    return "".join(c if c.isalnum() or c in ("_", "-", ".") else "_" for c in name)


def export_texture(asset_data):
    asset_name = asset_data.asset_name
    package_path = asset_data.package_name
    rel = package_path.replace("/Game/", "").replace("/", "_")
    out_name = sanitize_name(f"{rel}_{asset_name}.png")
    out_path = os.path.join(EXPORT_ROOT, out_name)

    task = unreal.AssetExportTask()
    task.object = asset_data.get_asset()
    task.filename = out_path
    task.selected = False
    task.replace_identical = True
    task.prompt = False
    task.automated = True
    task.exporter = unreal.TextureExporterPNG()
    ok = unreal.Exporter.run_asset_export_tasks([task])
    if ok:
        unreal.log(f"[ShaderViewerExport] Exported {package_path}.{asset_name} -> {out_path}")
    else:
        unreal.log_warning(f"[ShaderViewerExport] Failed to export {package_path}.{asset_name}")


def main():
    ensure_dir(EXPORT_ROOT)
    asset_registry = unreal.AssetRegistryHelpers.get_asset_registry()
    filter = unreal.ARFilter(
        class_names=["Texture2D"],
        recursive_paths=True,
        package_paths=[CONTENT_ROOT]
    )
    assets = asset_registry.get_assets(filter)
    unreal.log(f"[ShaderViewerExport] Found {len(assets)} Texture2D assets under {CONTENT_ROOT}")
    for asset_data in assets:
        try:
            export_texture(asset_data)
        except Exception as exc:
            unreal.log_error(f"[ShaderViewerExport] Error exporting {asset_data.asset_name}: {exc}")
    unreal.log(f"[ShaderViewerExport] Done. Files written to {EXPORT_ROOT}")


if __name__ == "__main__":
    main()
