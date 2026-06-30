@echo off
title Suzuki ECU Update Server
cd /d "E:\suzuki-ecu-tool\server"
echo Starting Update Server on Port 80...
python update_server.py 80
pause
