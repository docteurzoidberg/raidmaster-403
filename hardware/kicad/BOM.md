# BOM des cartes du raidmaster 403

Composants des deux cartes (carte fille, carte boutons), lus dans les schémas KiCad de
`carte-fille/` : **fichier généré, ne pas modifier à la main** en dehors du bloc « Hors schéma ».
La section d'un composant vient de son champ `Variante` (`commun`, `interne`,
`puck`) ; la carte boutons est commune aux deux versions. Non monté = attribut DNP, hors BOM ou
« DNP » dans la valeur.

<!-- sources : `carte-fille.kicad_sch` 2e0da2376db3, `carte-boutons.kicad_sch` c4587fbc4d48 -->

## Version GPS intégré (NEO-M9N sur la carte fille)

### Carte fille (26 composants)

| Qté | Repères | Type | Valeur | Boîtier | Rôle |
|---:|---|---|---|---|---|
| 3 | C1, C4, C7 | Condensateur | 100nF | 0805 |  |
| 2 | C2, C3 | Condensateur | 10uF | 0805 |  |
| 2 | C5, C6 | Condensateur | 1uF | 0805 |  |
| 1 | R1 | Résistance | 22R 0,75W | 2010 |  |
| 1 | R2 | Résistance | 100k | 0805 |  |
| 2 | R3, R4 | Résistance | 33R | 0805 |  |
| 1 | R5 | Résistance | 1k | 0805 |  |
| 2 | R6, R7 | Résistance | 4k7 | 0805 |  |
| 1 | R8 | Résistance | 10k | 0805 |  |
| 1 | R9 | Résistance | 510R | 0805 |  |
| 1 | L1 | Inductance | 27nH | 0805 |  |
| 1 | D1 | Diode / LED | PGB1010603 | 0603 |  |
| 1 | D2 | Diode / LED | LED PPS | 0805 |  |
| 1 | J1 | Connecteur | Waveshare J8 | PinHeader_2x16_P2.54mm_Vertical | 2x16 mâle |
| 1 | J2 | Connecteur | U.FL | U.FL_Hirose_U.FL-R-SMT-1_Vertical |  |
| 1 | J3 | Connecteur | GY-271 | PinHeader_1x05_P2.54mm_Vertical | QMC5883L |
| 1 | J4 | Connecteur | Nappe boutons | TE_84952-6_1x06-1MP_P1.0mm_Horizontal | FFC 6 P1.0 ZIF |
| 1 | U1 | Circuit intégré | NEO-M9N | ublox_NEO |  |
| 1 | U2 | Circuit intégré | AP2112K-3.3 | SOT-23-5 |  |
| 1 | U3 | Circuit intégré | XC6206P302MR | SOT-23 |  |

Pastilles du PCB, rien à acheter : JP1, TP1, TP2, TP3, TP4, TP5, TP6.

### Carte boutons (20 composants)

| Qté | Repères | Type | Valeur | Boîtier | Rôle |
|---:|---|---|---|---|---|
| 5 | SW101, SW102, SW103, SW104, SW105 | Bouton | Poussoir 12 × 12 | SW_PUSH-12mm | SW101 : BTN1 ; SW102 : BTN2 ; SW103 : BTN3 ; SW104 : BTN4 ; SW105 : BTN5 |
| 2 | C101, C103 | Condensateur | 100nF | 0805 |  |
| 1 | C102 | Condensateur | 10uF | 0805 |  |
| 1 | R102 | Résistance | 10k | 0805 |  |
| 4 | R103, R104, R105, R106 | Résistance | 150R | 0805 |  |
| 2 | D101, D102 | Diode / LED | Bicolore R/V 3mm | LED_D3.0mm-3 | D101 : PWR ; D102 : GPS |
| 1 | J101 | Connecteur | Nappe | TE_84952-6_1x06-1MP_P1.0mm_Horizontal | FFC 6 P1.0 ZIF |
| 1 | Q101 | Transistor | AO3401A | SOT-23 |  |
| 1 | U101 | Circuit intégré | SRV05-4 | SOT-23-6 |  |
| 2 | U102, U103 | Circuit intégré | PCF8574T | SOIC-16W_7.5x10.3mm_P1.27mm | U102 : boutons, 0x20 ; U103 : LED, 0x21 |

**Total : 46 composants à souder.**

## Version puck GPS externe (composants minimaux)

### Carte fille (20 composants)

| Qté | Repères | Type | Valeur | Boîtier | Rôle |
|---:|---|---|---|---|---|
| 2 | C2, C3 | Condensateur | 10uF | 0805 |  |
| 3 | C4, C7, C8 | Condensateur | 100nF | 0805 |  |
| 1 | R2 | Résistance | 100k | 0805 |  |
| 2 | R3, R4 | Résistance | 33R | 0805 |  |
| 2 | R6, R7 | Résistance | 4k7 | 0805 |  |
| 1 | R8 | Résistance | 10k | 0805 |  |
| 1 | R9 | Résistance | 510R | 0805 |  |
| 1 | R15 | Résistance | 0R | 0805 | puck : +3V3_GNSS |
| 1 | J1 | Connecteur | Waveshare J8 | PinHeader_2x16_P2.54mm_Vertical | 2x16 mâle |
| 1 | J3 | Connecteur | GY-271 | PinHeader_1x05_P2.54mm_Vertical | QMC5883L |
| 1 | J4 | Connecteur | Nappe boutons | TE_84952-6_1x06-1MP_P1.0mm_Horizontal | FFC 6 P1.0 ZIF |
| 1 | J6 | Connecteur | Puck GPS | JST_PH_S6B-PH-K_1x06_P2.00mm_Horizontal | JST-PH 6 P2.0 traversant |
| 1 | U2 | Circuit intégré | AP2112K-3.3 | SOT-23-5 |  |
| 1 | U7 | Circuit intégré | SRV05-4 | SOT-23-6 |  |
| 1 | U8 | Circuit intégré | TCA4307 | VSSOP-8_3x3mm_P0.65mm | tampon I2C du puck |

Non montés (empreinte prévue) : R16 0R DNP (puck : VBAT); R17 4k7 DNP (rappel SDA puck); R18 4k7 DNP (rappel SCL puck).
Pastilles du PCB, rien à acheter : TP2, TP3, TP4, TP5, TP6.

### Carte boutons (20 composants)

| Qté | Repères | Type | Valeur | Boîtier | Rôle |
|---:|---|---|---|---|---|
| 5 | SW101, SW102, SW103, SW104, SW105 | Bouton | Poussoir 12 × 12 | SW_PUSH-12mm | SW101 : BTN1 ; SW102 : BTN2 ; SW103 : BTN3 ; SW104 : BTN4 ; SW105 : BTN5 |
| 2 | C101, C103 | Condensateur | 100nF | 0805 |  |
| 1 | C102 | Condensateur | 10uF | 0805 |  |
| 1 | R102 | Résistance | 10k | 0805 |  |
| 4 | R103, R104, R105, R106 | Résistance | 150R | 0805 |  |
| 2 | D101, D102 | Diode / LED | Bicolore R/V 3mm | LED_D3.0mm-3 | D101 : PWR ; D102 : GPS |
| 1 | J101 | Connecteur | Nappe | TE_84952-6_1x06-1MP_P1.0mm_Horizontal | FFC 6 P1.0 ZIF |
| 1 | Q101 | Transistor | AO3401A | SOT-23 |  |
| 1 | U101 | Circuit intégré | SRV05-4 | SOT-23-6 |  |
| 2 | U102, U103 | Circuit intégré | PCF8574T | SOIC-16W_7.5x10.3mm_P1.27mm | U102 : boutons, 0x20 ; U103 : LED, 0x21 |

**Total : 40 composants à souder.**

## Hors schéma (à acheter avec les cartes)

<!-- manuel:debut -->
_(Tenu à jour à la main, conservé d'une génération à l'autre.)_

| Version | Qté | Article | Notes |
|---|---:|---|---|
| les deux | 1 | Nappe FFC 6 voies, pas de 1,0 mm, **type B** (contacts sur faces opposées) | entre J4 et J101 ; ~50 mm, longueur à fixer |
| les deux | 1 | Module GY-271 (QMC5883L) | sur J3, soudé à plat côté GPS, sans barrette ; optionnel |
| GPS intégré | 1 | Antenne GPS active, prise SMA | |
| GPS intégré | 1 | Pigtail U.FL → SMA femelle de cloison | de J2 à la SMA tenue par le manchon d'antenne |
| puck | 1 | Puck GPS de drone (NEO-M8N + boussole, 4 + 2 fils) | fiche à refaire en JST-PH 6 pour J6 |

La carte Waveshare ESP32-S3-Touch-LCD-3.5B, la batterie, le haut-parleur et la visserie du boîtier ne
sont pas dans cette BOM.

<!-- manuel:fin -->
