CREATE TABLE measure_list
(
    line_id     VARCHAR(40),
    tire_model  VARCHAR(40),
    ins_time    DATETIME,
    real_ecc    DOUBLE,
    real_dir    DOUBLE,
    eccentric   DOUBLE,
    direction   DOUBLE
);

CREATE TAGDATA TABLE tag
(
    line_id         VARCHAR(40) PRIMARY KEY,
    tick_time       DATETIME BASETIME, 
    sensor_value    DOUBLE SUMMARIZED,
    encoder_value   INT
);

