#include "Writing.h"

Writing::Writing(double time, double brightness, double frequency_correction)
	: time(time), brightness(brightness), frequency_correction(frequency_correction) {}

double Writing::getTime() const {
	return time;
}

double Writing::getBrightness() const {
	return brightness;
}

double Writing::getFrequencyCorrection() const {
	return frequency_correction;
}

Node *Writing::getNode() const {
	return node;
}

void Writing::setTime(double time) {
	this->time = time;
}

void Writing::setBrightness(double brightness) {
	this->brightness = brightness;
}

void Writing::setFrequencyCorrection(double frequency_correction) {
	this->frequency_correction = frequency_correction;
}

void Writing::setNode(Node *node) {
	this->node = node;
}

void Writing::deteriorate() {
	brightness *= DETERIORATION;
}
