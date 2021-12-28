/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2020 Soeren Apel <soeren@apelpie.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <extdef.h>

#include <QComboBox>
#include <QDebug>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QString>
#include <QTabWidget>
#include <QVBoxLayout>

#include "mathsignal.hpp"

#include "pv/data/signalbase.hpp"

using pv::data::SignalBase;

namespace pv {
namespace views {
namespace trace {

#define MATHSIGNAL_INPUT_TIMEOUT (2000)

const vector< pair<string, string> > MathEditDialog::Examples = {
	{"Product of signals named A1 and A2:",
	 "A1 * A2"},
	{"Create a sine wave with f=1MHz:",
	 "sin(1e6 * 2 * pi * t)"},
	{"Average the past 4 samples of A2:",
	 "var v := 0;\n" \
	 "var n := 4;\n\n" \
	 "for (var i := 0; i < n; i += 1) {\n" \
	 "\tv += sample('A2', s - i) / n;\n" \
	 "}"},
	{"Create a <a href=https://en.wikipedia.org/wiki/Chirp#Linear>frequency sweep</a> from 1Hz to 1MHz over 10 seconds:",
	 "var f_min := 1;\n" \
	 "var f_max := 1e6;\n" \
	 "var duration := 10;\n" \
	 "var gradient := (f_max - f_min) / duration;\n" \
	 "// phase is the antiderivative of (gradient * t + f_min) dt\n" \
	 "var phase := gradient * (t ^ 2 / 2) + f_min * t;\n" \
	 "sin(2 * pi * phase)"},
	{"Single-pole low-pass IIR filter, see " \
	 "<a href=https://tomroelandts.com/articles/low-pass-single-pole-iir-filter>this</a> " \
	 "and <a href=https://dsp.stackexchange.com/questions/34969/cutoff-frequency-of-a-first-order-recursive-filter>this</a> article:",
	 "// -3db point is set to 10% of the sampling frequency\n" \
	 "var f_c := 0.1;\n" \
	 "var omega_c := 2 * pi * f_c;\n" \
	 "var b := 2 - cos(omega_c) - sqrt(((2 - cos(omega_c)) ^ 2) - 1);\n" \
	 "var a := 1 - b;\n\n" \
	 "// formula is y[n] = ax[n] + (1 - a) * y[n - 1]\n" \
	 "// x[n] becomes the input signal, here A4\n" \
	 "// y[n - 1] becomes sample('Math1', s - 1)\n" \
	 "a * A4 + (1 - a) * sample('Math1', s - 1)"}
};


MathEditDialog::MathEditDialog(pv::Session &session,
	shared_ptr<pv::data::MathSignal> math_signal, QWidget *parent) :
	QDialog(parent),
	session_(session),
	math_signal_(math_signal),
	old_expression_(math_signal->get_expression()),
	expr_edit_(new QPlainTextEdit())
{
	setWindowTitle(tr("Math Expression Editor"));

	// Create the pages
	QWidget *basics_page = new QWidget();
	QVBoxLayout *basics_layout = new QVBoxLayout(basics_page);
	basics_layout->addWidget(new QLabel("<b>" + tr("Inputs:") + "</b>"));
	basics_layout->addWidget(new QLabel("signal_name\tCurrent value of signal signal_name (e.g. A0 or CH1)"));
	basics_layout->addWidget(new QLabel("T\t\tSampling interval (= 1 / sample rate)"));
	basics_layout->addWidget(new QLabel("t\t\tCurrent time in seconds"));
	basics_layout->addWidget(new QLabel("s\t\tCurrent sample number"));
	basics_layout->addWidget(new QLabel("sample('s', n)\tValue of sample #n of the signal named s"));
	basics_layout->addWidget(new QLabel("<b>" + tr("Variables:") + "</b>"));
	basics_layout->addWidget(new QLabel("var x;\t\tCreate variable\nvar x := 5;\tCreate variable with initial value"));
	basics_layout->addWidget(new QLabel("<b>" + tr("Basic operators:") + "</b>"));
	basics_layout->addWidget(new QLabel("x + y\t\tAddition of x and y"));
	basics_layout->addWidget(new QLabel("x - y\t\tSubtraction of x and y"));
	basics_layout->addWidget(new QLabel("x * y\t\tMultiplication of x and y"));
	basics_layout->addWidget(new QLabel("x / y\t\tDivision of x and y"));
	basics_layout->addWidget(new QLabel("x % y\t\tRemainder of division x / y"));
	basics_layout->addWidget(new QLabel("x ^ y\t\tx to the power of y"));
	basics_layout->addWidget(new QLabel("<b>" + tr("Assignments:") + "</b>"));
	basics_layout->addWidget(new QLabel("x := y\t\tAssign the value of y to x"));
	basics_layout->addWidget(new QLabel("x += y\t\tIncrement x by y"));
	basics_layout->addWidget(new QLabel("x -= y\t\tDecrement x by y"));
	basics_layout->addWidget(new QLabel("x *= y\t\tMultiply x by y"));
	basics_layout->addWidget(new QLabel("x /= y\t\tDivide x by y"));
	basics_layout->addWidget(new QLabel("x %= y\t\tSet x to the value of x % y"));

	QWidget *func1_page = new QWidget();
	QVBoxLayout *func1_layout = new QVBoxLayout(func1_page);
	func1_layout->addWidget(new QLabel("<b>" + tr("General purpose functions:") + "</b>"));
	func1_layout->addWidget(new QLabel(tr("abs(x)\t\tAbsolute value of x")));
	func1_layout->addWidget(new QLabel(tr("avg(x, y, ...)\tAverage of all input values")));
	func1_layout->addWidget(new QLabel(tr("ceil(x)\t\tSmallest integer that is greater than or equal to x")));
	func1_layout->addWidget(new QLabel(tr("clamp(lb, x, ub)\tClamp x in range between lb and ub, where lb < ub")));
	func1_layout->addWidget(new QLabel(tr("equal(x, y)\tEquality test between x and y using normalised epsilon")));
	func1_layout->addWidget(new QLabel(tr("erf(x)\t\tError function of x")));
	func1_layout->addWidget(new QLabel(tr("erfc(x)\t\tComplimentary error function of x")));
	func1_layout->addWidget(new QLabel(tr("exp(x)\t\te to the power of x")));
	func1_layout->addWidget(new QLabel(tr("expm1(x)\te to the power of x minus 1, where x is very small.")));
	func1_layout->addWidget(new QLabel(tr("floor(x)\t\tLargest integer that is less than or equal to x")));
	func1_layout->addWidget(new QLabel(tr("frac(x)\t\tFractional portion of x")));
	func1_layout->addWidget(new QLabel(tr("hypot(x)\t\tHypotenuse of x and y (i.e. sqrt(x*x + y*y))")));
	func1_layout->addWidget(new QLabel(tr("iclamp(lb, x, ub)\tInverse-clamp x outside of the range lb and ub, where lb < ub.\n\t\tIf x is within the range it will snap to the closest bound")));
	func1_layout->addWidget(new QLabel(tr("inrange(lb, x, ub)\tIn-range returns true when x is within the range lb and ub, where lb < ub.")));
	func1_layout->addWidget(new QLabel(tr("log(x)\t\tNatural logarithm of x")));
	func1_layout->addWidget(new QLabel(tr("log10(x)\t\tBase 10 logarithm of x")));

	QWidget *func2_page = new QWidget();
	QVBoxLayout *func2_layout = new QVBoxLayout(func2_page);
	func2_layout->addWidget(new QLabel(tr("log1p(x)\t\tNatural logarithm of 1 + x, where x is very small")));
	func2_layout->addWidget(new QLabel(tr("log2(x)\t\tBase 2 logarithm of x")));
	func2_layout->addWidget(new QLabel(tr("logn(x)\t\tBase N logarithm of x, where n is a positive integer")));
	func2_layout->addWidget(new QLabel(tr("max(x, y, ...)\tLargest value of all the inputs")));
	func2_layout->addWidget(new QLabel(tr("min(x, y, ...)\tSmallest value of all the inputs")));
	func2_layout->addWidget(new QLabel(tr("mul(x, y, ...)\tProduct of all the inputs")));
	func2_layout->addWidget(new QLabel(tr("ncdf(x)\t\tNormal cumulative distribution function")));
	func2_layout->addWidget(new QLabel(tr("nequal(x, y)\tNot-equal test between x and y using normalised epsilon")));
	func2_layout->addWidget(new QLabel(tr("pow(x, y)\tx to the power of y")));
	func2_layout->addWidget(new QLabel(tr("root(x, n)\tNth-Root of x, where n is a positive integer")));
	func2_layout->addWidget(new QLabel(tr("round(x)\t\tRound x to the nearest integer")));
	func2_layout->addWidget(new QLabel(tr("roundn(x, n)\tRound x to n decimal places, where n > 0 and is an integer")));
	func2_layout->addWidget(new QLabel(tr("sgn(x)\t\tSign of x; -1 if x < 0, +1 if x > 0, else zero")));
	func2_layout->addWidget(new QLabel(tr("sqrt(x)\t\tSquare root of x, where x >= 0")));
	func2_layout->addWidget(new QLabel(tr("sum(x, y, ..,)\tSum of all the inputs")));
	func2_layout->addWidget(new QLabel(tr("swap(x, y)\tSwap the values of the variables x and y and return the current value of y")));
	func2_layout->addWidget(new QLabel(tr("trunc(x)\t\tInteger portion of x")));

	QWidget *trig_page = new QWidget();
	QVBoxLayout *trig_layout = new QVBoxLayout(trig_page);
	trig_layout->addWidget(new QLabel("<b>" + tr("Trigonometry functions:") + "</b>"));
	trig_layout->addWidget(new QLabel(tr("acos(x)\t\tArc cosine of x expressed in radians. Interval [-1,+1]")));
	trig_layout->addWidget(new QLabel(tr("acosh(x)\t\tInverse hyperbolic cosine of x expressed in radians")));
	trig_layout->addWidget(new QLabel(tr("asin(x)\t\tArc sine of x expressed in radians. Interval [-1,+1]")));
	trig_layout->addWidget(new QLabel(tr("asinh(x)\t\tInverse hyperbolic sine of x expressed in radians")));
	trig_layout->addWidget(new QLabel(tr("atan(x)\t\tArc tangent of x expressed in radians. Interval [-1,+1]")));
	trig_layout->addWidget(new QLabel(tr("atan2(x, y)\tArc tangent of (x / y) expressed in radians. [-pi,+pi]  ")));
	trig_layout->addWidget(new QLabel(tr("atanh(x)\t\tInverse hyperbolic tangent of x expressed in radians")));
	trig_layout->addWidget(new QLabel(tr("cos(x)\t\tCosine of x")));
	trig_layout->addWidget(new QLabel(tr("cosh(x)\t\tHyperbolic cosine of x")));
	trig_layout->addWidget(new QLabel(tr("cot(x)\t\tCotangent of x")));
	trig_layout->addWidget(new QLabel(tr("csc(x)\t\tCosectant of x")));
	trig_layout->addWidget(new QLabel(tr("sec(x)\t\tSecant of x")));
	trig_layout->addWidget(new QLabel(tr("sin(x)\t\tSine of x")));
	trig_layout->addWidget(new QLabel(tr("sinc(x)\t\tSine cardinal of x")));
	trig_layout->addWidget(new QLabel(tr("sinh(x)\t\tHyperbolic sine of x")));
	trig_layout->addWidget(new QLabel(tr("tan(x)\t\tTangent of x")));
	trig_layout->addWidget(new QLabel(tr("tanh(x)\t\tHyperbolic tangent of x")));
	trig_layout->addWidget(new QLabel(tr("deg2rad(x)\tConvert x from degrees to radians")));
	trig_layout->addWidget(new QLabel(tr("deg2grad(x)\tConvert x from degrees to gradians")));
	trig_layout->addWidget(new QLabel(tr("rad2deg(x)\tConvert x from radians to degrees")));
	trig_layout->addWidget(new QLabel(tr("grad2deg(x)\tConvert x from gradians to degrees")));

	QWidget *logic_page = new QWidget();
	QVBoxLayout *logic_layout = new QVBoxLayout(logic_page);
	logic_layout->addWidget(new QLabel("<b>" + tr("Logic operators:") + "</b>"));
	logic_layout->addWidget(new QLabel("true\t\tTrue state or any value other than zero (typically 1)"));
	logic_layout->addWidget(new QLabel("false\t\tFalse state, value of exactly zero"));
	logic_layout->addWidget(new QLabel("x and y\t\tLogical AND, true only if x and y are both true"));
	logic_layout->addWidget(new QLabel("mand(x, y, z, ...)\tMulti-input logical AND, true only if all inputs are true"));
	logic_layout->addWidget(new QLabel("x or y\t\tLogical OR, true if either x or y is true"));
	logic_layout->addWidget(new QLabel("mor(x, y, z, ...)\tMulti-input logical OR, true if at least one of the inputs is true"));
	logic_layout->addWidget(new QLabel("x nand y\t\tLogical NAND, true only if either x or y is false"));
	logic_layout->addWidget(new QLabel("x nor y\t\tLogical NOR, true only if the result of x or y is false"));
	logic_layout->addWidget(new QLabel("not(x)\t\tLogical NOT, negate the logical sense of the input"));
	logic_layout->addWidget(new QLabel("x xor y\t\tLogical NOR, true only if the logical states of x and y differ"));
	logic_layout->addWidget(new QLabel("x xnor y\t\tLogical NOR, true only if x and y have the same state"));
	logic_layout->addWidget(new QLabel("x & y\t\tSame as AND but with left to right expression short circuiting optimisation"));
	logic_layout->addWidget(new QLabel("x | y\t\tSame as OR but with left to right expression short circuiting optimisation"));

	QWidget *control1_page = new QWidget();
	QVBoxLayout *control1_layout = new QVBoxLayout(control1_page);
	control1_layout->addWidget(new QLabel("<b>" + tr("Comparisons:") + "</b>"));
	control1_layout->addWidget(new QLabel(tr("x = y or x == y\tTrue only if x is strictly equal to y")));
	control1_layout->addWidget(new QLabel(tr("x <> y or x != y\tTrue only if x does not equal y")));
	control1_layout->addWidget(new QLabel(tr("x < y\t\tTrue only if x is less than y")));
	control1_layout->addWidget(new QLabel(tr("x <= y\t\tTrue only if x is less than or equal to y")));
	control1_layout->addWidget(new QLabel(tr("x > y\t\tTrue only if x is greater than y")));
	control1_layout->addWidget(new QLabel(tr("x >= y\t\tTrue only if x is greater than or equal to y")));
	control1_layout->addWidget(new QLabel("<b>" + tr("Flow control:") + "</b>"));
	control1_layout->addWidget(new QLabel(tr("{ ... }\t\tBeginning and end of instruction block")));
	control1_layout->addWidget(new QLabel(tr("if (x, y, z)\tIf x is true then return y else return z\nif (x) y;\t\t\tvariant without implied else\nif (x) { y };\t\tvariant with an instruction block\nif (x) y; else z;\t\tvariant with explicit else\nif (x) { y } else { z };\tvariant with instruction blocks")));
	control1_layout->addWidget(new QLabel(tr("x ? y : z\tTernary operator, equivalent to 'if (x, y, z)'")));
	control1_layout->addWidget(new QLabel(tr("switch {\t\tThe first true case condition that is encountered will\n case x > 1: a;\tdetermine the result of the switch. If none of the case\n case x < 1: b;\tconditions hold true, the default action is used\n default:     c;\tto determine the return value\n}")));

	QWidget *control2_page = new QWidget();
	QVBoxLayout *control2_layout = new QVBoxLayout(control2_page);
	control2_layout->addWidget(new QLabel(tr("while (conditon) {\tEvaluates expression repeatedly as long as condition is true,\n expression;\treturning the last value of expression\n}")));
	control2_layout->addWidget(new QLabel(tr("repeat\t\tEvalues expression repeatedly as long as condition is false,\n expression;\treturning the last value of expression\nuntil (condition)\n")));
	control2_layout->addWidget(new QLabel(tr("for (var x := 0; condition; x += 1) {\tRepeatedly evaluates expression while the condition is true,\n expression\t\t\twhile evaluating the 'increment' expression on each loop\n}")));
	control2_layout->addWidget(new QLabel(tr("break\t\tTerminates the execution of the nearest enclosed loop, returning NaN")));
	control2_layout->addWidget(new QLabel(tr("break[x]\t\tTerminates the execution of the nearest enclosed loop, returning x")));
	control2_layout->addWidget(new QLabel(tr("continue\t\tInterrupts loop execution and resumes with the next loop iteration")));
	control2_layout->addWidget(new QLabel(tr("return[x]\t\tReturns immediately from within the current expression, returning x")));
	control2_layout->addWidget(new QLabel(tr("~(expr; expr; ...)\tEvaluates each sub-expression and returns the value of the last one\n~{expr; expr; ...}")));

	QWidget *example_page = new QWidget();
	QVBoxLayout *example_layout = new QVBoxLayout(example_page);
	for (const pair<string, string> &entry : Examples) {
		const string &desc = entry.first;
		const string &example = entry.second;

		QLabel *desc_label = new QLabel("<b>" + tr(desc.c_str()) + "</b>");
		desc_label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
		desc_label->setOpenExternalLinks(true);
		example_layout->addWidget(desc_label);
		QPushButton *btn = new QPushButton(tr("Copy to expression"));
		connect(btn, &QPushButton::clicked, this, [&]() { set_expr(QString::fromStdString(example)); });
		QGridLayout *gridlayout = new QGridLayout();
		gridlayout->addWidget(new QLabel(QString::fromStdString(example)), 0, 0, Qt::AlignLeft);
		gridlayout->addWidget(btn, 0, 1, Qt::AlignRight);
		example_layout->addLayout(gridlayout);
	}

	// Create the rest of the dialog
	QDialogButtonBox *button_box = new QDialogButtonBox(
		QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

	QTabWidget *tabs = new QTabWidget();
	tabs->addTab(basics_page, tr("Basics"));
	tabs->addTab(func1_page, tr("Functions 1"));
	tabs->addTab(func2_page, tr("Functions 2"));
	tabs->addTab(trig_page, tr("Trigonometry"));
	tabs->addTab(logic_page, tr("Logic"));
	tabs->addTab(control1_page, tr("Flow Control 1"));
	tabs->addTab(control2_page, tr("Flow Control 2"));
	tabs->addTab(example_page, tr("Examples"));

	QVBoxLayout *root_layout = new QVBoxLayout(this);
	root_layout->addWidget(tabs);
	root_layout->addWidget(expr_edit_);
	root_layout->addWidget(button_box);

	// Set tab width to 4 characters
	expr_edit_->setTabStopDistance(util::text_width(QFontMetrics(font()), "XXXX"));

	connect(button_box, SIGNAL(accepted()), this, SLOT(accept()));
	connect(button_box, SIGNAL(rejected()), this, SLOT(reject()));
}

void MathEditDialog::set_expr(const QString &expr)
{
	expr_edit_->document()->setPlainText(expr);
}

void MathEditDialog::accept()
{
	math_signal_->set_expression(expr_edit_->document()->toPlainText());
	QDialog::accept();
}

void MathEditDialog::reject()
{
	math_signal_->set_expression(old_expression_);
	QDialog::reject();
}


MathSignal::MathSignal(
	pv::Session &session,
	shared_ptr<data::SignalBase> base) :
	AnalogSignal(session, base),
	math_signal_(dynamic_pointer_cast<pv::data::MathSignal>(base))
{
	delayed_expr_updater_.setSingleShot(true);
	delayed_expr_updater_.setInterval(MATHSIGNAL_INPUT_TIMEOUT);
	connect(&delayed_expr_updater_, &QTimer::timeout,
		this, [this]() { math_signal_->set_expression(expression_edit_->text()); });
}

void MathSignal::populate_popup_form(QWidget *parent, QFormLayout *form)
{
	AnalogSignal::populate_popup_form(parent, form);

	expression_edit_ = new QLineEdit();
	expression_edit_->setText(math_signal_->get_expression());

	const QIcon edit_icon(QIcon::fromTheme("edit", QIcon(":/icons/math.svg")));
	QAction *edit_action =
		expression_edit_->addAction(edit_icon, QLineEdit::TrailingPosition);

	connect(expression_edit_, SIGNAL(textEdited(QString)),
		this, SLOT(on_expression_changed(QString)));
	connect(edit_action, SIGNAL(triggered(bool)),
		this, SLOT(on_edit_clicked()));
	form->addRow(tr("Expression"), expression_edit_);

	sample_count_cb_ = new QComboBox();
	sample_count_cb_->setEditable(true);
	sample_count_cb_->addItem(tr("same as session"));
	sample_count_cb_->addItem(tr("100"));
	sample_count_cb_->addItem(tr("10000"));
	sample_count_cb_->addItem(tr("1000000"));
	connect(sample_count_cb_, SIGNAL(editTextChanged(QString)),
		this, SLOT(on_sample_count_changed(QString)));
	form->addRow(tr("Number of Samples"), sample_count_cb_);

	sample_rate_cb_ = new QComboBox();
	sample_rate_cb_->setEditable(true);
	sample_rate_cb_->addItem(tr("same as session"));
	sample_rate_cb_->addItem(tr("100"));
	sample_rate_cb_->addItem(tr("10000"));
	sample_rate_cb_->addItem(tr("1000000"));
	form->addRow(tr("Sample rate"), sample_rate_cb_);
}

void MathSignal::on_expression_changed(const QString &text)
{
	(void)text;

	// Restart update timer
	delayed_expr_updater_.start();
}

void MathSignal::on_sample_count_changed(const QString &text)
{
	(void)text;
}

void MathSignal::on_edit_clicked()
{
	MathEditDialog dlg(session_, math_signal_);
	dlg.set_expr(expression_edit_->text());

	dlg.exec();
}

} // namespace trace
} // namespace views
} // namespace pv
