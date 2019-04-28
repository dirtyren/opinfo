
CREATE DATABASE opnetdb;

USE opnetdb;

--
-- Table structure for table `history`
--

DROP TABLE IF EXISTS `history`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `history` (
  `hostname` varchar(100) DEFAULT NULL,
  `status` int(11) DEFAULT NULL,
  `date` datetime DEFAULT NULL
);
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `ophosts`
--

DROP TABLE IF EXISTS `ophosts`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `ophosts` (
  `hostname` varchar(100) NOT NULL DEFAULT '',
  `mac_address` varchar(20) DEFAULT NULL,
  `ip` varchar(15) DEFAULT NULL,
  `description` varchar(200) DEFAULT NULL,
  `dyn_dns` int(11) DEFAULT '1',
  `date_registered` datetime DEFAULT NULL,
  `date_last_update` datetime DEFAULT NULL,
  `monitor` int(11) DEFAULT '1',
  `link_provider` varchar(100) DEFAULT NULL,
  `circuit_number` varchar(100) DEFAULT NULL,
  `address` varchar(200) DEFAULT NULL,
  `city` varchar(100) DEFAULT NULL,
  `state` varchar(100) DEFAULT NULL,
  `country` varchar(100) DEFAULT NULL,
  `reboot` int(11) DEFAULT '0',
  `hw_change` int(11) DEFAULT '0',
  `kernel_version` varchar(50) DEFAULT NULL,
  `opnet_load` varchar(10) DEFAULT NULL,
  `gen_dns` int(11) DEFAULT '0',
  `laststatus` int(11) DEFAULT '1',
  `opnet` int(11) DEFAULT '1',
  `ssh_port` varchar(10) DEFAULT NULL,
  `backup_time` int(11) DEFAULT '1',
  `backup_last` datetime DEFAULT NULL,
  `hwinfo` text,
  `http_port` varchar(10) DEFAULT NULL,
  PRIMARY KEY (`hostname`)
);
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

-- Dump completed on 2019-04-28 10:49:10
