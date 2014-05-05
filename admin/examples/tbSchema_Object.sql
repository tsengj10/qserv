-- MySQL dump 10.13  Distrib 5.1.61, for redhat-linux-gnu (x86_64)
--
-- Host: localhost    Database: qservTest_case01_m
-- ------------------------------------------------------
-- Server version	5.1.61-log

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8 */;
/*!40103 SET @OLD_TIME_ZONE=@@TIME_ZONE */;
/*!40103 SET TIME_ZONE='+00:00' */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;

--
-- Table structure for table `Object`
--

DROP TABLE IF EXISTS `Object`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `Object` (
  `objectId` bigint(20) NOT NULL,
  `iauId` char(34) DEFAULT NULL,
  `ra_PS` double NOT NULL,
  `ra_PS_Sigma` float DEFAULT NULL,
  `decl_PS` double NOT NULL,
  `decl_PS_Sigma` float DEFAULT NULL,
  `radecl_PS_Cov` float DEFAULT NULL,
  `htmId20` bigint(20) NOT NULL,
  `ra_SG` double DEFAULT NULL,
  `ra_SG_Sigma` float DEFAULT NULL,
  `decl_SG` double DEFAULT NULL,
  `decl_SG_Sigma` float DEFAULT NULL,
  `radecl_SG_Cov` float DEFAULT NULL,
  `raRange` float DEFAULT NULL,
  `declRange` float DEFAULT NULL,
  `muRa_PS` double DEFAULT NULL,
  `muRa_PS_Sigma` float DEFAULT NULL,
  `muDecl_PS` double DEFAULT NULL,
  `muDecl_PS_Sigma` float DEFAULT NULL,
  `muRaDecl_PS_Cov` float DEFAULT NULL,
  `parallax_PS` double DEFAULT NULL,
  `parallax_PS_Sigma` float DEFAULT NULL,
  `canonicalFilterId` tinyint(4) DEFAULT NULL,
  `extendedness` smallint(6) DEFAULT NULL,
  `varProb` float DEFAULT NULL,
  `earliestObsTime` double DEFAULT NULL,
  `latestObsTime` double DEFAULT NULL,
  `meanObsTime` double DEFAULT NULL,
  `flags` int(11) DEFAULT NULL,
  `uNumObs` int(11) DEFAULT NULL,
  `uExtendedness` smallint(6) DEFAULT NULL,
  `uVarProb` float DEFAULT NULL,
  `uRaOffset_PS` float DEFAULT NULL,
  `uRaOffset_PS_Sigma` float DEFAULT NULL,
  `uDeclOffset_PS` float DEFAULT NULL,
  `uDeclOffset_PS_Sigma` float DEFAULT NULL,
  `uRaDeclOffset_PS_Cov` float DEFAULT NULL,
  `uRaOffset_SG` float DEFAULT NULL,
  `uRaOffset_SG_Sigma` float DEFAULT NULL,
  `uDeclOffset_SG` float DEFAULT NULL,
  `uDeclOffset_SG_Sigma` float DEFAULT NULL,
  `uRaDeclOffset_SG_Cov` float DEFAULT NULL,
  `uLnL_PS` float DEFAULT NULL,
  `uLnL_SG` float DEFAULT NULL,
  `uFlux_PS` float DEFAULT NULL,
  `uFlux_PS_Sigma` float DEFAULT NULL,
  `uFlux_ESG` float DEFAULT NULL,
  `uFlux_ESG_Sigma` float DEFAULT NULL,
  `uFlux_Gaussian` float DEFAULT NULL,
  `uFlux_Gaussian_Sigma` float DEFAULT NULL,
  `uTimescale` float DEFAULT NULL,
  `uEarliestObsTime` double DEFAULT NULL,
  `uLatestObsTime` double DEFAULT NULL,
  `uSersicN_SG` float DEFAULT NULL,
  `uSersicN_SG_Sigma` float DEFAULT NULL,
  `uE1_SG` float DEFAULT NULL,
  `uE1_SG_Sigma` float DEFAULT NULL,
  `uE2_SG` float DEFAULT NULL,
  `uE2_SG_Sigma` float DEFAULT NULL,
  `uRadius_SG` float DEFAULT NULL,
  `uRadius_SG_Sigma` float DEFAULT NULL,
  `uFlags` int(11) DEFAULT NULL,
  `gNumObs` int(11) DEFAULT NULL,
  `gExtendedness` smallint(6) DEFAULT NULL,
  `gVarProb` float DEFAULT NULL,
  `gRaOffset_PS` float DEFAULT NULL,
  `gRaOffset_PS_Sigma` float DEFAULT NULL,
  `gDeclOffset_PS` float DEFAULT NULL,
  `gDeclOffset_PS_Sigma` float DEFAULT NULL,
  `gRaDeclOffset_PS_Cov` float DEFAULT NULL,
  `gRaOffset_SG` float DEFAULT NULL,
  `gRaOffset_SG_Sigma` float DEFAULT NULL,
  `gDeclOffset_SG` float DEFAULT NULL,
  `gDeclOffset_SG_Sigma` float DEFAULT NULL,
  `gRaDeclOffset_SG_Cov` float DEFAULT NULL,
  `gLnL_PS` float DEFAULT NULL,
  `gLnL_SG` float DEFAULT NULL,
  `gFlux_PS` float DEFAULT NULL,
  `gFlux_PS_Sigma` float DEFAULT NULL,
  `gFlux_ESG` float DEFAULT NULL,
  `gFlux_ESG_Sigma` float DEFAULT NULL,
  `gFlux_Gaussian` float DEFAULT NULL,
  `gFlux_Gaussian_Sigma` float DEFAULT NULL,
  `gTimescale` float DEFAULT NULL,
  `gEarliestObsTime` double DEFAULT NULL,
  `gLatestObsTime` double DEFAULT NULL,
  `gSersicN_SG` float DEFAULT NULL,
  `gSersicN_SG_Sigma` float DEFAULT NULL,
  `gE1_SG` float DEFAULT NULL,
  `gE1_SG_Sigma` float DEFAULT NULL,
  `gE2_SG` float DEFAULT NULL,
  `gE2_SG_Sigma` float DEFAULT NULL,
  `gRadius_SG` float DEFAULT NULL,
  `gRadius_SG_Sigma` float DEFAULT NULL,
  `gFlags` int(11) DEFAULT NULL,
  `rNumObs` int(11) DEFAULT NULL,
  `rExtendedness` smallint(6) DEFAULT NULL,
  `rVarProb` float DEFAULT NULL,
  `rRaOffset_PS` float DEFAULT NULL,
  `rRaOffset_PS_Sigma` float DEFAULT NULL,
  `rDeclOffset_PS` float DEFAULT NULL,
  `rDeclOffset_PS_Sigma` float DEFAULT NULL,
  `rRaDeclOffset_PS_Cov` float DEFAULT NULL,
  `rRaOffset_SG` float DEFAULT NULL,
  `rRaOffset_SG_Sigma` float DEFAULT NULL,
  `rDeclOffset_SG` float DEFAULT NULL,
  `rDeclOffset_SG_Sigma` float DEFAULT NULL,
  `rRaDeclOffset_SG_Cov` float DEFAULT NULL,
  `rLnL_PS` float DEFAULT NULL,
  `rLnL_SG` float DEFAULT NULL,
  `rFlux_PS` float DEFAULT NULL,
  `rFlux_PS_Sigma` float DEFAULT NULL,
  `rFlux_ESG` float DEFAULT NULL,
  `rFlux_ESG_Sigma` float DEFAULT NULL,
  `rFlux_Gaussian` float DEFAULT NULL,
  `rFlux_Gaussian_Sigma` float DEFAULT NULL,
  `rTimescale` float DEFAULT NULL,
  `rEarliestObsTime` double DEFAULT NULL,
  `rLatestObsTime` double DEFAULT NULL,
  `rSersicN_SG` float DEFAULT NULL,
  `rSersicN_SG_Sigma` float DEFAULT NULL,
  `rE1_SG` float DEFAULT NULL,
  `rE1_SG_Sigma` float DEFAULT NULL,
  `rE2_SG` float DEFAULT NULL,
  `rE2_SG_Sigma` float DEFAULT NULL,
  `rRadius_SG` float DEFAULT NULL,
  `rRadius_SG_Sigma` float DEFAULT NULL,
  `rFlags` int(11) DEFAULT NULL,
  `iNumObs` int(11) DEFAULT NULL,
  `iExtendedness` smallint(6) DEFAULT NULL,
  `iVarProb` float DEFAULT NULL,
  `iRaOffset_PS` float DEFAULT NULL,
  `iRaOffset_PS_Sigma` float DEFAULT NULL,
  `iDeclOffset_PS` float DEFAULT NULL,
  `iDeclOffset_PS_Sigma` float DEFAULT NULL,
  `iRaDeclOffset_PS_Cov` float DEFAULT NULL,
  `iRaOffset_SG` float DEFAULT NULL,
  `iRaOffset_SG_Sigma` float DEFAULT NULL,
  `iDeclOffset_SG` float DEFAULT NULL,
  `iDeclOffset_SG_Sigma` float DEFAULT NULL,
  `iRaDeclOffset_SG_Cov` float DEFAULT NULL,
  `iLnL_PS` float DEFAULT NULL,
  `iLnL_SG` float DEFAULT NULL,
  `iFlux_PS` float DEFAULT NULL,
  `iFlux_PS_Sigma` float DEFAULT NULL,
  `iFlux_ESG` float DEFAULT NULL,
  `iFlux_ESG_Sigma` float DEFAULT NULL,
  `iFlux_Gaussian` float DEFAULT NULL,
  `iFlux_Gaussian_Sigma` float DEFAULT NULL,
  `iTimescale` float DEFAULT NULL,
  `iEarliestObsTime` double DEFAULT NULL,
  `iLatestObsTime` double DEFAULT NULL,
  `iSersicN_SG` float DEFAULT NULL,
  `iSersicN_SG_Sigma` float DEFAULT NULL,
  `iE1_SG` float DEFAULT NULL,
  `iE1_SG_Sigma` float DEFAULT NULL,
  `iE2_SG` float DEFAULT NULL,
  `iE2_SG_Sigma` float DEFAULT NULL,
  `iRadius_SG` float DEFAULT NULL,
  `iRadius_SG_Sigma` float DEFAULT NULL,
  `iFlags` int(11) DEFAULT NULL,
  `zNumObs` int(11) DEFAULT NULL,
  `zExtendedness` smallint(6) DEFAULT NULL,
  `zVarProb` float DEFAULT NULL,
  `zRaOffset_PS` float DEFAULT NULL,
  `zRaOffset_PS_Sigma` float DEFAULT NULL,
  `zDeclOffset_PS` float DEFAULT NULL,
  `zDeclOffset_PS_Sigma` float DEFAULT NULL,
  `zRaDeclOffset_PS_Cov` float DEFAULT NULL,
  `zRaOffset_SG` float DEFAULT NULL,
  `zRaOffset_SG_Sigma` float DEFAULT NULL,
  `zDeclOffset_SG` float DEFAULT NULL,
  `zDeclOffset_SG_Sigma` float DEFAULT NULL,
  `zRaDeclOffset_SG_Cov` float DEFAULT NULL,
  `zLnL_PS` float DEFAULT NULL,
  `zLnL_SG` float DEFAULT NULL,
  `zFlux_PS` float DEFAULT NULL,
  `zFlux_PS_Sigma` float DEFAULT NULL,
  `zFlux_ESG` float DEFAULT NULL,
  `zFlux_ESG_Sigma` float DEFAULT NULL,
  `zFlux_Gaussian` float DEFAULT NULL,
  `zFlux_Gaussian_Sigma` float DEFAULT NULL,
  `zTimescale` float DEFAULT NULL,
  `zEarliestObsTime` double DEFAULT NULL,
  `zLatestObsTime` double DEFAULT NULL,
  `zSersicN_SG` float DEFAULT NULL,
  `zSersicN_SG_Sigma` float DEFAULT NULL,
  `zE1_SG` float DEFAULT NULL,
  `zE1_SG_Sigma` float DEFAULT NULL,
  `zE2_SG` float DEFAULT NULL,
  `zE2_SG_Sigma` float DEFAULT NULL,
  `zRadius_SG` float DEFAULT NULL,
  `zRadius_SG_Sigma` float DEFAULT NULL,
  `zFlags` int(11) DEFAULT NULL,
  `yNumObs` int(11) DEFAULT NULL,
  `yExtendedness` smallint(6) DEFAULT NULL,
  `yVarProb` float DEFAULT NULL,
  `yRaOffset_PS` float DEFAULT NULL,
  `yRaOffset_PS_Sigma` float DEFAULT NULL,
  `yDeclOffset_PS` float DEFAULT NULL,
  `yDeclOffset_PS_Sigma` float DEFAULT NULL,
  `yRaDeclOffset_PS_Cov` float DEFAULT NULL,
  `yRaOffset_SG` float DEFAULT NULL,
  `yRaOffset_SG_Sigma` float DEFAULT NULL,
  `yDeclOffset_SG` float DEFAULT NULL,
  `yDeclOffset_SG_Sigma` float DEFAULT NULL,
  `yRaDeclOffset_SG_Cov` float DEFAULT NULL,
  `yLnL_PS` float DEFAULT NULL,
  `yLnL_SG` float DEFAULT NULL,
  `yFlux_PS` float DEFAULT NULL,
  `yFlux_PS_Sigma` float DEFAULT NULL,
  `yFlux_ESG` float DEFAULT NULL,
  `yFlux_ESG_Sigma` float DEFAULT NULL,
  `yFlux_Gaussian` float DEFAULT NULL,
  `yFlux_Gaussian_Sigma` float DEFAULT NULL,
  `yTimescale` float DEFAULT NULL,
  `yEarliestObsTime` double DEFAULT NULL,
  `yLatestObsTime` double DEFAULT NULL,
  `ySersicN_SG` float DEFAULT NULL,
  `ySersicN_SG_Sigma` float DEFAULT NULL,
  `yE1_SG` float DEFAULT NULL,
  `yE1_SG_Sigma` float DEFAULT NULL,
  `yE2_SG` float DEFAULT NULL,
  `yE2_SG_Sigma` float DEFAULT NULL,
  `yRadius_SG` float DEFAULT NULL,
  `yRadius_SG_Sigma` float DEFAULT NULL,
  `yFlags` int(11) DEFAULT NULL,
  `chunkId` int(11) DEFAULT NULL,
  `subChunkId` int(11) DEFAULT NULL
) ENGINE=MyISAM DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

-- Dump completed on 2012-03-28 10:31:56