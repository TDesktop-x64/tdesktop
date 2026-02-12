# Code Review Style Guide

This file contains style and formatting rules that the review subagent must check and fix. These are mechanical issues that should always be caught during code review.

## Empty line before closing brace

Always add an empty line before the closing brace of a class (after all private fields):

```cpp
// BAD:
class MyClass {
public:
    void foo();

private:
    int _value = 0;
};

// GOOD:
class MyClass {
public:
    void foo();

private:
    int _value = 0;

};
```

## Multi-line expressions — operators at the start of continuation lines

When splitting an expression across multiple lines, place operators (like `&&`, `||`, `;`, `+`, etc.) at the **beginning** of continuation lines, not at the end of the previous line. This makes it immediately obvious from the left edge whether a line is a continuation or new code.

```cpp
// BAD - continuation looks like scope code:
if (const auto &lottie = animation->lottie;
	lottie && lottie->valid() && lottie->framesCount() > 1) {
	lottie->animate([=] {

// GOOD - semicolon at start signals continuation:
if (const auto &lottie = animation->lottie
	; lottie && lottie->valid() && lottie->framesCount() > 1) {
	lottie->animate([=] {

// BAD - trailing && makes next line look like independent code:
if (veryLongExpression() &&
	anotherLongExpression() &&
	anotherOne()) {
	doSomething();

// GOOD - leading && clearly marks continuation:
if (veryLongExpression()
	&& anotherLongExpression()
	&& anotherOne()) {
	doSomething();
```

## Minimize type checks — prefer direct cast over is + as

Don't check a type and then cast — just cast and check for null. `asUser()` already returns `nullptr` when the peer is not a user, so calling `isUser()` first is redundant. The same applies to `asChannel()`, `asChat()`, etc.

```cpp
// BAD - redundant isUser() check, then asUser():
if (peer && peer->isUser()) {
	peer->asUser()->setNoForwardFlags(

// GOOD - just cast and null-check:
if (const auto user = peer->asUser()) {
	user->setNoForwardFlags(
```

When you need a specific subtype, look up the specific subtype directly instead of loading a generic type and then casting:

```cpp
// BAD - loads generic peer, then casts:
if (const auto peer = session().data().peerLoaded(peerId)
	; peer && peer->isUser()) {
	peer->asUser()->setNoForwardFlags(

// GOOD - look up the specific subtype directly:
const auto userId = peerToUser(peerId);
if (const auto user = session().data().userLoaded(userId)) {
	user->setNoForwardFlags(
```

Avoid C++17 `if` with initializer (`;` inside the condition) when the code can be written more clearly with simple nested `if` statements or by extracting the value beforehand:

```cpp
// BAD - complex if-with-initializer:
if (const auto peer = session().data().peerLoaded(peerId)
	; peer && peer->isUser()) {

// GOOD - simple nested ifs when direct lookup isn't available:
if (const auto peer = session().data().peerLoaded(peerId)) {
	if (const auto user = peer->asUser()) {

## std::optional access — avoid value()

Do not call `std::optional::value()` because it throws an exception that is not available on older macOS targets. Use `has_value()`, `value_or()`, `operator bool()`, or `operator*` instead.
```
