@echo off
cd /d "C:\Program Files\Bambu Studio"
"bambu-studio.exe" --debug 3 --load-settings "d:\projects\Programming\Git\chess_esp32_c6_devkit\models\chess_pieces\slice\machine.json;d:\projects\Programming\Git\chess_esp32_c6_devkit\models\chess_pieces\slice\process.json" --load-filaments "d:\projects\Programming\Git\chess_esp32_c6_devkit\models\chess_pieces\slice\filament.json" --curr-bed-type "Textured PEI Plate" --arrange 1 --min-save --slice 1 --outputdir "d:\projects\Programming\Git\chess_esp32_c6_devkit\models\chess_pieces\slice" --export-3mf "white_h2d_pla.3mf" "d:\projects\Programming\Git\chess_esp32_c6_devkit\models\chess_pieces\stl\white.stl" > "d:\projects\Programming\Git\chess_esp32_c6_devkit\models\chess_pieces\slice\slice_log.txt" 2>&1
echo EXIT %ERRORLEVEL%>> "d:\projects\Programming\Git\chess_esp32_c6_devkit\models\chess_pieces\slice\slice_log.txt"
echo EXIT %ERRORLEVEL%
