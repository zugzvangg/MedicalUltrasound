#include "Solver.h"
#include "Constants.h"

#include "json.hpp"
#include <fstream>
#include <iostream>

Solver::Solver() {
	init();
	propagate();
}

void Solver::init() {
	initObstacles();
	initDots();
	nlohmann::json jsonData;
    std::ifstream config("../res/config.json");
    config >> jsonData;
	rays_num = jsonData["SETUP_BASE"]["RAYS_NUM"];
	focus =   jsonData["SETUP_BASE"]["FOCUS"];
	PIES = jsonData["SETUP_BASE"]["PIES"];
	// nodesNum = jsonData["SETUP_BASE"]["NODES_NUM"];
	//может понадобиться, оно почему-то не считывалось, может быть тут 
	// что -то как раз ломалось 
}

void Solver::initObstacles() {
	nlohmann::json jsonData;
    std::ifstream config("../res/config.json");
    config >> jsonData;
	OBSTACLES= jsonData["OBSTACLES"]["NUMBER_OF_OBSTACLES"];

	for (int i = 0; i < OBSTACLES; i++) {
		Obstacle obstacle;
		for (int j = 0; j < VERTICES; j++) {
			int x = jsonData['OBSTACLES']['X'][i];
			int y = jsonData['OBSTACLES']['Y'][i];
			Vector2 myVec(x, y);
			obstacle.addPos(myVec);
		}
		double c_rel = jsonData['OBSTACLES']['C_REL'];
		obstacle.setCRel(c_rel);
		obstacle.addPos(obstacle.getPos(0));
		obstacles.push_back(obstacle);
	}
}

void Solver::initDots() {
	nlohmann::json jsonData;
    std::ifstream config("../res/config.json");
    config >> jsonData;
	DOTS =  jsonData["DOTS"]["NUMBER_OF_DOTS"];

	for (int i = 0; i < DOTS; i++) {
		Dot dot;
		int x = jsonData['DOTS']['X'][i];
		int y = jsonData['DOTS']['Y'][i];
		double brightness = jsonData['DOTS']['B'][i];
		dot.setPos(Vector2(x, y));
		dot.setBrightness(brightness);
		dots.push_back(dot);
	}
}

void Solver::propagate() {
	for (int i = 0; i < SENSORS; i++) {
		double x = X / 2 - DX_SENSORS * (SENSORS / 2 - i);
		double y = Y * 0.999;
		Sensor sensor;
		sensor.setPos(Vector2(x, y));
		sensors.push_back(sensor);
	}
	for (auto &sensor : sensors) {
		initExplosion(sensor.getPos());
		resetTime();
		while (finishTime > 0) {
			step();
		}
	}
}

void Solver::initExplosion(Vector2 pos) {
	nodesNum = 0;
	int n = POINTS_IN_DOT_WAVEFRONT * 2;

	for (int i = 0; i < n; i++) {
		double angle = 2.0 * M_PI * i / (double) n;
		Vector2 velocity(cos(angle), sin(angle));
		nodes[nodesNum++] = new Node(pos, velocity);
	}

	for (int i = 1; i < n - 1; i++) {
		nodes[i]->setLeft(nodes[i - 1]);
		nodes[i]->setRight(nodes[i + 1]);
	}

	nodes[0]->setRight(nodes[1]);
	nodes[0]->setLeft(nodes[n - 1]);
	nodes[n - 1]->setLeft(nodes[n - 2]);
	nodes[n - 1]->setRight(nodes[0]);

	for (auto &sensor : sensors) {
		sensor.clearWriting();
	}

}

void Solver::step() {
	for (int node = 0; node < nodesNum; node++) {
		int encounters = 0;
		if (nodes[node]->getTEncounter() == INFINITY) {
			encounters += checkObstacles(node);
			if (nodes[node]->getRight()) {
				encounters += checkDots(node);
			}

			if (!encounters) {
				nodes[node]->setTEncounter(-1);
			}
		}
	}

	double timeStep = DT_DIGITIZATION * T_MULTIPLIER;
	for (int node = 0; node < nodesNum; node++) {
		if (nodes[node]->getTEncounter() > -0.5) {
			timeStep = std::min(timeStep, nodes[node]->getTEncounter());
		}
	}
	for (int node = 0; node < nodesNum; node++) {
		nodes[node]->update(timeStep, obstacles[nodes[node]->getMaterial()].getCRel());
	}
	handleReflection();
	fixNodes();

	deteriorationTime += timeStep;
	while (deteriorationTime > DT_DETERIORATION) {
		deteriorate();
		deteriorationTime -= DT_DETERIORATION;
	}

	totalTime += timeStep;
	while (totalTime > DT_DIGITIZATION) {
		if (startTime < 0) {
			writeToCSV();
		}
		totalTime -= DT_DIGITIZATION;
		startTime -= DT_DIGITIZATION;
		finishTime -= DT_DIGITIZATION;
	}
}

int Solver::checkObstacles(int node) {
	int encounters = 0;
	double dist, time = INFINITY;

	for (int i = 0; i < OBSTACLES; i++) {
		for (int j = 0; j < VERTICES - 1; j++) {
			if (doIntersect(obstacles[i].getPos(j),
							obstacles[i].getPos(j + 1),
							nodes[node]->getPos(),
							nodes[node]->getVelocity(),
							&dist)) {

				time = nodes[node]->getTime(dist, obstacles[nodes[node]->getMaterial()].getCRel());

				if (time < nodes[node]->getTEncounter()) {
					nodes[node]->setTEncounter(time);
					nodes[node]->setObstacleNumber(i);
					nodes[node]->setVerticeNumber(j);
					encounters++;
				}
			}
		}
	}
	return encounters;
}

int Solver::checkDots(int node) {
	int encounters = 0;
	double time = INFINITY;

	Vector2 nodePos = nodes[node]->getPosAfterStep(10);
	Vector2 rightNodePos = nodes[node]->getRight()->getPosAfterStep(10);
	Vector2 nodeNextPos = nodes[node]->getPosAfterStep(1000);
	Vector2 rightNodeNextPos = nodes[node]->getRight()->getPosAfterStep(1000);

	for (int i = 0; i < DOTS; i++) {
		if (isPointInRect(dots[i].getPos(), nodePos, rightNodePos, nodeNextPos, rightNodeNextPos)) {
			double dist = distanceToSegment(nodes[node]->getPos(), nodes[node]->getRight()->getPos(), dots[i].getPos());

			time = nodes[node]->getTime(dist, obstacles[nodes[node]->getMaterial()].getCRel());

			if (time < nodes[node]->getTEncounter()) {
				nodes[node]->setTEncounter(time);
				nodes[node]->setObstacleNumber(-1);
				nodes[node]->setVerticeNumber(i);
				encounters++;
			}
		}
	}
	for (auto &sensor : sensors) {
		if (isPointInRect(sensor.getPos(), nodePos, rightNodePos, nodeNextPos, rightNodeNextPos)) {
			double dist = distanceToSegment(nodes[node]->getPos(), nodes[node]->getRight()->getPos(),
											sensor.getPos());

			time = nodes[node]->getTime(dist, obstacles[nodes[node]->getMaterial()].getCRel());

			std::vector<Writing> writing = sensor.getWriting();
			if (time < nodes[node]->getTEncounter()) {
				writing.push_back(Writing(-time, nodes[node]->getIntensity(),
										  1.0 / nodes[node]->getVelocity().getY()));
			}
			sensor.setWriting(writing);
		}
	}

	return encounters;
}

void Solver::handleReflection() {
	double delta = 0.5;
	for (int i = 0; i < nodesNum; i++) {
		if (nodes[i]->getTEncounter() < ZERO && nodes[i]->getTEncounter() > -delta) {
			if (nodes[i]->getObstacleNumber() >= 0) {
				Node reflected = nodes[i]->getReflected(obstacles[nodes[i]->getObstacleNumber()]);
				Node refracted = nodes[i]->getRefracted(obstacles[nodes[i]->getObstacleNumber()]);

				if (reflected.getIntensity() == -1) {
					nodes[i]->setMarkedForTheKill(1);
				} else {
					// real neighbors always turn to ghost ones -
					// reflected go to the other direction, refracted are in another material
					if (nodes[i]->getLeft()) {
						Node *left = nodes[i]->getLeft();
						reflected.addLeftVirtualNeighbor(left);
						refracted.addLeftVirtualNeighbor(left);
						left->addRightVirtualNeighbor(&reflected);
						left->addLeftVirtualNeighbor(&refracted);
						nodes[i]->setLeft(left);
					}
					if (nodes[i]->getRight()) {
						Node *right = nodes[i]->getRight();
						reflected.addRightVirtualNeighbor(right);
						refracted.addRightVirtualNeighbor(right);
						right->addRightVirtualNeighbor(&reflected);
						right->addLeftVirtualNeighbor(&refracted);
						nodes[i]->setRight(right);
					}

					nodes[i]->restoreWavefront(reflected, refracted);
				}
			} else if (nodes[i]->getObstacleNumber() == -1) {    // encountering a dot obstacle
				double sina = -nodes[i]->getVelocity().getY();
				double cosa = -nodes[i]->getVelocity().getX();
				double alpha = cosa > 0 ? asin(sina) : M_PI - asin(sina);
				double dalpha = M_PI / (POINTS_IN_DOT_WAVEFRONT - 1);

				alpha += M_PI / 2;
				int oldNodesNum = nodesNum;
				for (int j = 0; j < POINTS_IN_DOT_WAVEFRONT; j++) {
					Node *n = new Node(dots[nodes[i]->getVerticeNumber()], alpha);
					nodes[nodesNum++] = n;
					alpha -= dalpha;
				}
				for (int j = 1; j < POINTS_IN_DOT_WAVEFRONT - 1; j++) {
					nodes[oldNodesNum + j]->setLeft(nodes[oldNodesNum + j - 1]);
					nodes[oldNodesNum + j]->setRight(nodes[oldNodesNum + j + 1]);
				}
				nodes[oldNodesNum]->setRight(nodes[oldNodesNum + 1]);
				nodes[nodesNum - 1]->setLeft(nodes[nodesNum - 2]);
				nodes[i]->setTEncounter(INFINITY);
			}
		}
	}
}
void Solver::fixNodes() {
	for (int node = 0; node < nodesNum; node++) {
		nodes[node]->checkInvalid();

		if (nodes[node]->getMarkedForTheKill()) {
			for (auto &sensor : sensors) {
				for (int j = 0; j < sensor.getWriting().size(); j++) {
					if (sensor.getWriting()[j].getNode() == nodes[node]) {
						sensor.clearWriting();
					}
				}
			}
			delete nodes[node];
			nodes[node] = NULL;
		}
	}

	bool cleared = false;
	while (!cleared) {
		while (!nodes[nodesNum - 1] && nodesNum) {
			nodesNum--;
		}
		cleared = true;
		for (int i = 0; i < nodesNum; i++) {
			if (!nodes[i]) {
				nodes[i] = nodes[--nodesNum];
				cleared = false;
				break;
			}
		}
	}

	for (int node = 0; node < nodesNum; node++) {
		if (nodes[node]) {
			nodes[node]->clearNeighbours();
		}
	}
}
Solver::~Solver() {
	for (int i = 0; i < nodesNum; i++) {
		if (nodes[i] != NULL) {
			delete nodes[i];
		}
	}
}

void Solver::resetTime() {
	startTime = SENSORS * DX_SENSORS * 0.1;
	finishTime = 2.0 * Y;
}

void Solver::deteriorate() {
	for (int n = 0; n < nodesNum; n++) {
		nodes[n]->deteriorate();
	}
	for (auto &sensor : sensors) {
		sensor.deteriorate();
	}
}
void Solver::writeToCSV() {
	for (int i = 0; i < sensors.size(); i++) {
		std::ofstream csvFile;
		csvFile.open("data/baseline/Sensor" + std::to_string(i) + "/raw" + std::to_string(i) + ".csv");
		sensors[i].writeToCSV(csvFile);
		csvFile.close();
	}
}
void Solver::initSetting() {
    nlohmann::json jsonData;
    std::ifstream config("../res/config.json");
    config >> jsonData;

    VERTICES = jsonData["Constants"]["VERTICES"];
    POINTS_IN_DOT_WAVEFRONT = jsonData["Constants"]["POINTS_IN_DOT_WAVEFRONT"];
    OBSTACLES_TOTAL = jsonData["Constants"]["OBSTACLES_TOTAL"];
    DOTS_TOTAL = jsonData["Constants"]["DOTS_TOTAL"];
    SENSORS = jsonData["Constants"]["SENSORS"];
    DX_SENSORS = jsonData["Constants"]["DX_SENSORS"];
    ZERO = jsonData["Constants"]["ZERO"];
    X = jsonData["Constants"]["X"];
    Y = jsonData["Constants"]["Y"];
    T_MULTIPLIER = jsonData["Constants"]["T_MULTIPLIER"];
    DT_DIGITIZATION = jsonData["Constants"]["DT_DIGITIZATION"];
    MINLEN = jsonData["Constants"]["MINLEN"];
    DT_DETERIORATION = jsonData["Constants"]["DT_DETERIORATION"];
    DETERIORATION = jsonData["Constants"]["DETERIORATION"];
}
