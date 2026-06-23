{
  "board": {
    "design_settings": { "defaults": { "track_width": 0.254, "via_dia": 0.6, "via_drill": 0.3 } },
    "layer_presets": []
  },
  "schematic": {
    "version": 1,
    "components": [
      { "ref": "U1", "value": "Arduino_Nano", "footprint": "Module:Arduino_Nano" },
      { "ref": "U2", "value": "L9637D", "footprint": "Package_SO:SOIC-8_3.9x4.9mm_P1.27mm" },
      { "ref": "U3", "value": "4N35", "footprint": "Package_DIP:DIP-6_W7.62mm" },
      { "ref": "U4", "value": "LM7805", "footprint": "Package_TO-220:TO-220-3_Vertical" },
      { "ref": "R1", "value": "1k", "footprint": "Resistor_THT:R_Axial_DIN0204_L3.6mm_D1.6mm_P7.62mm" },
      { "ref": "R2", "value": "330", "footprint": "Resistor_THT:R_Axial_DIN0204_L3.6mm_D1.6mm_P7.62mm" },
      { "ref": "R3", "value": "10k", "footprint": "Resistor_THT:R_Axial_DIN0204_L3.6mm_D1.6mm_P7.62mm" },
      { "ref": "C1", "value": "100nF", "footprint": "Capacitor_THT:C_Disc_D3.0mm_W1.6mm_P2.50mm" },
      { "ref": "C2", "value": "100nF", "footprint": "Capacitor_THT:C_Disc_D3.0mm_W1.6mm_P2.50mm" },
      { "ref": "C3", "value": "10uF", "footprint": "Capacitor_THT:CP_Radial_D5.0mm_P2.50mm" },
      { "ref": "J1", "value": "6-pin_Sumitomo", "footprint": "Connector:6-pin-connector" }
    ],
    "nets": [
      { "name": "GND", "components_connected": ["U1-GND", "U2-1", "U2-8", "U3-2", "U3-4", "U4-2", "C1-2", "C2-2", "C3-2", "J1-3"] },
      { "name": "+5V", "components_connected": ["U1-5V", "U2-7", "U3-5_10k_R3-1", "U4-3", "C2-1"] },
      { "name": "+12V", "components_connected": ["U2-5", "U4-1", "R1-1", "C1-1", "J1-2"] },
      { "name": "TX", "components_connected": ["U1-D1", "U2-2"] },
      { "name": "RX", "components_connected": ["U1-D0", "U2-3"] },
      { "name": "K-LINE", "components_connected": ["U2-4", "R1-2", "J1-1"] },
      { "name": "DEALER", "components_connected": ["U1-D4", "R2-1", "U3-1"] },
      { "name": "DM_OUT", "components_connected": ["U3-5", "R3-2", "J1-4"] }
    ]
  }
}
