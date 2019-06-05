SELECT * FROM measure_list;
SELECT line_id, COUNT(sensor_value) FROM tag GROUP BY line_id;

