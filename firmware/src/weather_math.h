#pragma once

// Funções puras de meteorologia (sem dependência de hardware).

// Altitude do sensor: terreno + andares + altura sobre o piso.
float sensorAltitudeM(float groundAltM, int floor, float floorHeightM, float sensorHeightM);

// Pressão reduzida ao nível do mar (hPa) a partir da pressão na estação.
float seaLevelPressureHpa(float stationHpa, float altitudeM);

// Ponto de orvalho (°C), fórmula de Magnus.
float dewPointC(float tempC, float humPct);

// Sensação térmica por calor (índice de calor NOAA), em °C.
// Abaixo de ~27 °C retorna praticamente a própria temperatura.
float heatIndexC(float tempC, float humPct);
