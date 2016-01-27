<?php namespace Archon\Tests\DataFrame\SQL;

use Archon\DataFrame;
use PDO;

class SQLDataFrameUnitTest extends \PHPUnit_Framework_TestCase
{

    public function testToSQL()
    {
        $df = DataFrame::fromArray([
            ['a' => 1, 'b' => 2, 'c' => 3],
            ['a' => 4, 'b' => 5, 'c' => 6],
            ['a' => 7, 'b' => 8, 'c' => 9],
        ]);

        $pdo = new PDO('sqlite::memory:');

        $pdo->exec("CREATE TABLE testTable (a TEXT, b TEXT, c TEXT);");
        $df->toSQL($pdo, 'testTable');
        $result = $pdo->query("SELECT * FROM testTable;")->fetchAll(PDO::FETCH_ASSOC);
        $this->assertEquals($result, $df->toArray());
        $pdo->exec("DROP TABLE testTable;");
    }

    public function testFromSQL()
    {
        $pdo = new PDO('sqlite::memory:');
        $pdo->exec("CREATE TABLE testFromSQL (x TEXT, y TEXT, z TEXT);");
        $pdo->exec("INSERT INTO testFromSQL (x, y, z) VALUES (1, 2, 3), (4, 5, 6), (7, 8, 9);");

        $df = DataFrame::fromSQL($pdo, "SELECT * FROM testFromSQL;");

        $pdo->exec("DROP TABLE testFromSQL;");

        $expected = [
            ['x' => 1, 'y' => 2, 'z' => 3],
            ['x' => 4, 'y' => 5, 'z' => 6],
            ['x' => 7, 'y' => 8, 'z' => 9],
        ];

        $this->assertEquals($expected, $df->toArray());
    }
}
