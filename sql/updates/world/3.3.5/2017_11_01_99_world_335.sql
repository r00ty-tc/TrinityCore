UPDATE `command` 
    SET `help` = 'Syntax: .respawn [range] [spawnid]\n \nRespawn all nearest creatures and GO without waiting for respawn time expiration.\n \nOptional range (yards) and spawnId parameters available' 
    WHERE `name` = 'respawn';
