# Vizuální Plán Zapojení (Visual Wiring Diagram)

Zde je schématické znázornění zapojení pomocí Mermaid diagramu. Toto slouží jako "blokové schéma" před tvorbou detailního schématu v CADu.

```mermaid
graph TD
    %% Styling
    classDef power fill:#f96,stroke:#333,stroke-width:2px;
    classDef logic fill:#69f,stroke:#333,stroke-width:2px;
    classDef motor fill:#9f6,stroke:#333,stroke-width:2px;
    classDef gnd fill:#333,stroke:#fff,stroke-width:2px,color:#fff;

    subgraph Power_System [Napájecí Systém]
        BatA[Baterie A - 3S 3500mAh]:::power
        BatB[Baterie B - 3S 3500mAh]:::power
        FuseA[Pojistka A - 30A]:::power
        FuseB[Pojistka B - 5A]:::power
        SwitchA[Main Switch A]:::power
        SwitchB[Main Switch B]:::power
    end

    subgraph Logic_Unit [Mozek - Logic]
        Jetson[Nvidia Jetson Orin]:::logic
        PCA[PCA9685 PWM Driver]:::logic
    end

    subgraph Drive_System [Pohon - High Power]
        DriverL[BTS7960 Driver L]:::motor
        DriverR[BTS7960 Driver R]:::motor
        MotorL((Motor L - JGB37)):::motor
        MotorR((Motor R - JGB37)):::motor
    end

    %% Power Connections
    BatA ==> SwitchA ==> FuseA ==> DriverL & DriverR
    BatB ==> SwitchB ==> FuseB ==> Jetson

    %% Logic Connections
    Jetson -- I2C (SDA/SCL) --> PCA
    Jetson -- 5V/GND (Logic Power) --> PCA

    %% Control Signals (PWM)
    PCA -- PWM 0 --> DriverL
    PCA -- PWM 1 --> DriverL
    PCA -- PWM 2 --> DriverR
    PCA -- PWM 3 --> DriverR

    %% Motor Output
    DriverL ==> MotorL
    DriverR ==> MotorR

    %% Grounding
    GND_Point((STAR GND)):::gnd
    BatA -.-> GND_Point
    BatB -.-> GND_Point
    Jetson -.-> GND_Point
    DriverL -.-> GND_Point
    DriverR -.-> GND_Point
```

## Popis Pinů (PCA9685 -> Drivers)
*   **Kanál 0:** Motor L - Vpřed (R_PWM na Driveru L)
*   **Kanál 1:** Motor L - Vzad (L_PWM na Driveru L)
*   **Kanál 2:** Motor R - Vpřed (R_PWM na Driveru R)
*   **Kanál 3:** Motor R - Vzad (L_PWM na Driveru R)
*   **Enable Piny:** R_EN a L_EN na Driverech připojeny trvale na 5V (nebo přes další kanály PCA, pokud chceme "Kill Switch" funkcionalitu).
